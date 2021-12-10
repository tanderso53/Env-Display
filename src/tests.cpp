#include "jsonparse.h"
#include "display-driver.h"

#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

extern "C" void popFields(int pdfd);

#define BOOST_TEST_MODULE env_display_test
#include <boost/test/included/unit_test.hpp>

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

BOOST_AUTO_TEST_CASE(jsonparse_test)
{
  struct datafield* df = NULL;

  // Initialization test
  BOOST_REQUIRE_NO_THROW(initializeData(infile));

  // Counting test
  BOOST_TEST(numDataFields() == 5);

  BOOST_REQUIRE_NO_THROW(df = getDataDump(df));

  // Check that data object is not null
  BOOST_TEST(df);

  // Check that data content is correct
  BOOST_WARN(strcmp(df[0].name, "ammonia") == 0);
  BOOST_WARN(strcmp(df[0].value, "423") == 0);
  BOOST_WARN(strcmp(df[0].time, "1602546") == 0);
  BOOST_WARN(strcmp(df[0].unit, "counts") == 0);
  BOOST_WARN(strcmp(df[1].name, "temp") == 0);
  BOOST_WARN(strcmp(df[1].value, "23.18") == 0);
  BOOST_WARN(strcmp(df[1].time, "1602551") == 0);
  BOOST_WARN(strcmp(df[1].unit, "degC") == 0);

  // Check clearing of data
  BOOST_CHECK_NO_THROW(clearData());
}

// Forms module unit

BOOST_AUTO_TEST_CASE(forms_display_test)
{
  int fd;
  const char* tempfile = "/tmp/env-display_test";

  fd = open(tempfile, O_CREAT | O_RDWR);

  if (fd < 0) {
    BOOST_TEST_WARN(false, "Could not open file for testing");
    return;
  }

  if (write(fd, infile, strlen(infile)) < 0) {
    BOOST_TEST_WARN(false, "Could not write temp file");
    close(fd);
    return;
  }
  close(fd);

  fd = open(tempfile, O_CREAT | O_RDWR);

  if (fd < 0) {
    BOOST_TEST_WARN(false, "Could not open file for testing");
    return;
  }

  BOOST_CHECK_NO_THROW(popFields(fd));
  BOOST_CHECK_NO_THROW(formExit());

  close(fd);
}
