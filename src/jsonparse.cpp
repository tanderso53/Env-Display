#include "jsonparse.h"

#include <json/json.h>

#include <cstring>
#include <cmath>
#include <cstdio>
#include <csignal>
#include <cassert>

#include <iostream>
#include <vector>
#include <string>
#include <sstream>

struct datafield** _df = NULL;

class mrparser {
public:
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<std::string> units;
  std::vector<std::string> millis;
};

typedef std::vector<mrparser> parsedlist;

static parsedlist parsedvalues;

static void loadData(const Json::Value& ds, mrparser& parsed)
{
  for (unsigned int i = 0; i < ds["data"].size(); ++i) {
    const Json::Value& thisdata = ds["data"][i];
    char thisbuf[32];

    // Round values to a precision of 2
    snprintf(thisbuf, 32, "%.2f", thisdata["value"].asDouble());

    // Load parsed parameters into storage vectors
    parsed.names.push_back(thisdata["name"].asString());
    parsed.values.push_back(thisbuf);
    parsed.units.push_back(thisdata["unit"].asString());
    parsed.millis.push_back(thisdata["timemillis"].asString());
  }
}

void initializeData(const char* data)
{
  std::stringstream dstr(data);
  Json::Value ds;

  try {
    std::istream& blah = dstr;
    blah >> ds;
  }
  catch (std::exception& e) {
    std::cerr << "In initializeData(): Failed Json parse with: "
	      << e.what() << '\n'
	      << "Data is: " << data << '\n';
    raise(SIGABRT);
  }

  // Determine data format and allocate parsedvalues as required
  if (ds.isMember("data") && ds["data"].isArray()) {

    parsedvalues = parsedlist(1);
    loadData(ds, parsedvalues[0]);

  } else if (ds.isMember("output") && ds["output"].isArray()) {

    Json::Value& o = ds["output"];

    parsedvalues = parsedlist(o.size());

    for (auto it = parsedvalues.begin(); it != parsedvalues.end(); ++it) {
      loadData(o, *it);
    }

  } else {

    std::cerr << "In initializeData(): Unknown data format\n";
    raise(SIGABRT);

  }

}

int numDataFields(size_t i)
{
  return parsedvalues[i].names.size();
}

size_t numSensors()
{
  return parsedvalues.size();
}

void clearData()
{
  // Keep clearing array until NULL is reached
  if (_df) {

    for (size_t i = 0; i < numSensors(); ++i) {

      if (_df[i]) {

	free(_df[i]);

      }

    }

    free(_df);
  }

  _df = NULL;

  parsedvalues.clear();
}

struct datafield** getDataDump(struct datafield** df)
{
  if (_df) {
    df = _df;

    return df;
  }

  _df = (struct datafield**) malloc(numSensors() * sizeof(struct datafield*));

  assert(_df);

  for (size_t i = 0; i < numSensors(); ++i) {
    struct datafield *dfi;

    _df[i] = (struct datafield*) malloc(numDataFields(i) * sizeof(struct datafield));

    assert(_df[i]);

    dfi = _df[i];

    for (int j = 0; j < numDataFields(i); ++j) {
      strncpy(dfi[j].name, parsedvalues[i].names[j].c_str(), 32);
      strncpy(dfi[j].value, parsedvalues[i].values[j].c_str(), 32);
      strncpy(dfi[j].time, parsedvalues[i].millis[j].c_str(), 32);
      strncpy(dfi[j].unit, parsedvalues[i].units[j].c_str(), 32);
    }
  }

  df = _df;

  return df;
}
