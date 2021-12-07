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

struct datafield* _df = NULL;

class mrparser {
public:
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<std::string> units;
  std::vector<std::string> millis;
} parsedvalues;

void initializeData(const char* data)
{
  std::stringstream dstr(data);
  std::istream& blah = dstr;
  Json::Value ds;

  try {
    blah >> ds;
  }
  catch (std::exception& e) {
    std::cerr << "In initializeData(): Failed Json parse with: "
	      << e.what() << '\n'
	      << "Data is: " << data << '\n';
    raise(SIGABRT);
  }

  //Json::Value& d = ds["data"];

  for (unsigned int i = 0; i < ds["data"].size(); ++i) {
    Json::Value& thisdata = ds["data"][i];
    char thisbuf[32];

    // Round values to a precision of 2
    snprintf(thisbuf, 32, "%.2f", thisdata["value"].asDouble());

    // Load parsed parameters into storage vectors
    parsedvalues.names.push_back(thisdata["name"].asString());
    parsedvalues.values.push_back(thisbuf);
    parsedvalues.units.push_back(thisdata["unit"].asString());
    parsedvalues.millis.push_back(thisdata["timemillis"].asString());
  }
}

int numDataFields()
{
  return parsedvalues.names.size();
}

void clearData()
{
  parsedvalues.names.clear();
  parsedvalues.values.clear();
  parsedvalues.units.clear();
  parsedvalues.millis.clear();
  free(_df);
}

struct datafield* getDataDump(struct datafield* df)
{
  _df = (struct datafield*) malloc(numDataFields() * sizeof(struct datafield));

  for (int i = 0; i < numDataFields(); ++i) {
    strncpy(_df[i].name, parsedvalues.names[i].c_str(), 32);
    strncpy(_df[i].value, parsedvalues.values[i].c_str(), 32);
    strncpy(_df[i].time, parsedvalues.millis[i].c_str(), 32);
    strncpy(_df[i].unit, parsedvalues.units[i].c_str(), 32);
  }

  df = _df;

  return df;
}
