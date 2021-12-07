#include "jsonparse.h"

#include <iostream>
#include <cstring>

// Sample data string
const char* infile = "{\"status\": {\"isWarmedUp\": false, \"CCS811\": \"ok\", "
  "\"localIP\": \"192.168.1.211\", \"sentmillis\": 1602543}, \"data\": "
  "[{\"name\": \"ammonia\", \"value\": 423, \"timemillis\": 1602546, "
  "\"unit\": \"counts\"}, {\"name\": \"temp\", \"value\": 23.18, "
  "\"timemillis\": 1602551, \"unit\": \"degC\"}, {\"name\": \"pressure\", "
  "\"value\": 99492.27, \"timemillis\": 1602556, \"unit\": \"pa\"}, "
  "{\"name\": \"altitude\", \"value\": 153.69, \"timemillis\": 1602561, "
  "\"unit\": \"m\"}, {\"name\": \"rh\", \"value\": 43.76, \"timemillis\": 1602565, "
  "\"unit\": \"%\"}]}";

// json parse module unit

bool jsonparse_unit()
{
  int nfields, ret = 1;
  struct datafield* df = NULL;

  std::cout << "JSONPARSE UNIT ---- BEGIN\n";

  // Initialization test
  try {
    std::cout << "Attempting to initialize ... ";
    initializeData(infile);
  }
  catch (std::exception& e) {
    std::cout << "FAILURE\n" << "Got " << e.what();
    return 0;
  }

  std::cout << "SUCCESS\n";

  // Counting test
  std::cout << "Checking parse count (5): ";

  if ((nfields = numDataFields()) != 5) {
    std::cout << "Got " << nfields << " ... FAILURE\n";
    if (ret) {
      ret = 0;
    }
  }

  std::cout << " ... SUCCESS\n";

  df = getDataDump(df);

  // Check that data object is not null
  std::cout << "Checking for null datadump ... ";

  if (df) {
    std::cout << "SUCCESS\n";
  }
  else {
    std::cout << "FAILURE\n";

    if (ret) {
      return 0;
    }
  }

  // Check that data content is correct
  std::cout << "Checking for correct parsing ... ";

  int cmpr[] = {
    strcmp(df[0].name, "ammonia"),
    strcmp(df[0].value, "423"),
    strcmp(df[0].time, "1602546"),
    strcmp(df[0].unit, "counts"),
    strcmp(df[1].name, "temp"),
    strcmp(df[1].value, "23.18"),
    strcmp(df[1].time, "1602551"),
    strcmp(df[1].unit, "degC")
  };

  for (unsigned int i = 0; i < sizeof(cmpr)/sizeof(cmpr[0]); ++i) {
    if (cmpr[i] != 0) {
      std::cout << "FAILURE\n";
      ret = 0;
      break;
    }
  }

  if (ret) {
    std::cout << "SUCCESS\n";
  }

  // Check clearing of data
  std::cout << "Checking clearData(): ... ";
  clearData();

  if (!df) {
    std::cout << "SUCCESS\n";
  }
  else {
    std::cout << "FAILURE\n";
    ret = 0;
  }

  return ret;
}

int main()
{
  int succeeded = 0, total = 0;

  std::cout << "Initiating Tests:\n";

  if (jsonparse_unit()) {
    std::cout << " Jsonparse Unit ... SUCCESS\n";
    succeeded++;
  }
  else {
    std::cout << " Jsonparse Unit ... FAILURE\n";
  }

  total++;

  // Print results
  std::cout << succeeded << " Succeeded out of " << total << '\n';

  if (succeeded < total) {
    return 1;
  }

  return 0;
}
