#include "jsonparse.h"

#include <json/json.h>

#include <cstring>
#include <cmath>
#include <cstdio>
#include <csignal>

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

typedef std::unique_ptr<mrparser[]> parsedlist;

static parsedlist parsedvalues = nullptr;

static size_t nParsedValues = 0;

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

    parsedvalues.reset(new mrparser[1]);
    nParsedValues = 1;
    loadData(ds, parsedvalues[0]);

  } else if (ds.isMember("output") && ds["output"].isArray()) {

    Json::Value& o = ds["output"];

    parsedvalues.reset(new mrparser[o.size()]);
    nParsedValues = o.size();

    for (size_t i = 0; i < o.size(); ++i) {
      loadData(o, parsedvalues[i]);
    }

  } else {

    std::cerr << "In initializeData(): Unknown data format\n";
    raise(SIGABRT);

  }

  //Json::Value& d = ds["data"];

}

int numDataFields(size_t i)
{
  return parsedvalues[i].names.size();
}

size_t numSensors()
{
  return nParsedValues;
}

void clearData()
{
  parsedvalues.reset(nullptr);
  nParsedValues = 0;

  // Keep clearing array until NULL is reached
  if (_df) {
    for (size_t i = 0; _df[i]; ++i) {
      free(_df[i]);
    }

    free(_df);
  }
}

struct datafield** getDataDump(struct datafield** df)
{
  if (_df) {
    df = _df;

    return df;
  }

  _df = (struct datafield**) malloc(nParsedValues + 1 * sizeof(struct datafield*));

  for (size_t i = 0; i < nParsedValues; ++i) {
    _df[i] = (struct datafield*) malloc(numDataFields(i) * sizeof(struct datafield));
    struct datafield *dfi = _df[i];

    for (int j = 0; j < numDataFields(i); ++j) {
      strncpy(dfi->name, parsedvalues[i].names[j].c_str(), 32);
      strncpy(dfi->value, parsedvalues[i].values[j].c_str(), 32);
      strncpy(dfi->time, parsedvalues[i].millis[j].c_str(), 32);
      strncpy(dfi->unit, parsedvalues[i].units[j].c_str(), 32);
    }
  }

  _df[nParsedValues] = NULL;

  df = _df;

  return df;
}
