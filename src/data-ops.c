#include "data-ops.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <assert.h>

#ifndef DISPLAY_DRIVER_INPUT_BUFFER_LEN
#define DISPLAY_DRIVER_INPUT_BUFFER_LEN 4096
#endif

struct datafield errordf[] = {
	{
		.name = "ERROR"
	}
};

static int datafd = -1;

static struct metric *_metrics;
static struct metric_form _mf;

static void _allocateMetric(struct datafield **df);
static void _loadMetric(struct datafield **df);

struct datafield **pollData(struct datafield **df, uint32_t ms)
{
	assert(!df);
	assert(datafd >= 0);

	struct timespec to = {
		.tv_sec = 0,
		.tv_nsec = ms * 1000000
	};
	struct pollfd pfd = {
		.fd = datafd, /* open(openfile, O_RDWR), */
		.events = POLLIN
	};

	int pollresult = ppoll(&pfd, 1, &to, 0);

	if (pollresult < 0) {

		/* Dirty hack to prevent poll errors on window
		 * size changes */
		if (errno == EINTR) {
			return NULL;
		}

		perror("Critical Error polling file: ");
		return (struct datafield**) &errordf;
	}

	if (pollresult == 0) {
		return NULL;
	}

	return parseData(pfd.fd, df);
}

struct datafield **initialDataTransaction(int pdfd,
					  struct datafield **dfields)
{
	assert(!dfields);
	assert(pdfd >= 0);

	datafd = pdfd;

	struct pollfd pfd = {
		.fd = pdfd, /* open(parsefile, O_RDWR), */
		.events = POLLIN
	};

	printf("Polling for environmental data...\n");
	int pollresult = poll(&pfd, 1, 60000);

	if (pollresult < 0) {
		perror("Error while reading initial data: ");
		raise(SIGINT);
	}

	if (pollresult == 0) {
		fprintf(stderr, "Failed to get "
			"initial data from stream\n");
		raise(SIGINT);
	}

	return parseData(pdfd, dfields);
}


struct datafield **parseData(int pdfd, struct datafield** df)
{
	char buff[DISPLAY_DRIVER_INPUT_BUFFER_LEN];
	int i = 0;

	while (i < DISPLAY_DRIVER_INPUT_BUFFER_LEN - 1) {
		char c;

		int readresult = read(pdfd, &c, 1);

		if (readresult < 0) {
			perror("Error reading file: ");
			raise(SIGINT);
		}

		if (c == '\n' || c == '\r') {
			break;
		}

		if (readresult == 0) {
			struct pollfd pfd = {
				.fd = pdfd,
				.events = POLLIN
			};

			/* Keep polling if ran out of characters */
			int pollresult = poll(&pfd, 1, 1000);

			if (pollresult  > 0) {
				break;
			}

			continue;
		}

		buff[i] = c;
		++i;
	}

	buff[i] = '\0';

	initializeData(buff);
	return getDataDump(df);
}

bool isErrorDatafield(const struct datafield *df)
{
	if (!df)
		return false;

	return strcmp(df[0].name, errordf->name) == 0 &&
		strcmp(df[0].unit, errordf->unit) == 0 &&
		strcmp(df[0].value, errordf->value) == 0 &&
		strcmp(df[0].time, errordf->time) == 0;
}

int ncursesPollCB(long mstimeout)
{
	assert(_metrics);

	struct datafield **df = NULL;

	df = pollData(df, (uint32_t) mstimeout);

	if (!df)
		return 1;

	if (isErrorDatafield(df[0]))
		return -1;

	_loadMetric(df);

	clearData();

	return 0;
}

struct metric_form *ncursesCFG(int pdfd)
{
	struct datafield **df = NULL;

	df = initialDataTransaction(pdfd, df);

	_allocateMetric(df);

	_loadMetric(df);

	clearData();

	/* Set required cfg parameters */
	_mf.wd.pages = 1;
	_mf.metrics = _metrics;
	_mf.polldata_cb = ncursesPollCB;

	return &_mf;
}

void ncursesFreeMetric()
{
	if (_metrics)
		free(_metrics);
}

void _allocateMetric(struct datafield **df)
{
	assert(df);
	assert(!_metrics);

	int nfields = 1; /* Start at 1 for the empty one at the end */

	for (size_t i = 0; i < numSensors(); ++i) {
		for (int j = 0; j < numDataFields(i); ++j) {
			++nfields;
		}
	}

	_metrics = (struct metric*) malloc(nfields * sizeof(struct metric));
}

void _loadMetric(struct datafield **df)
{
	assert(df);

	int mi = 0;

	for (size_t i = 0; i < numSensors(); ++i) {
		struct datafield *dfi = df[i];

		for (int j = 0; j < numDataFields(i); ++j) {
			struct metric * addr = &_metrics[mi];

			strcpy(addr->name, dfi[j].name);
			strcpy(addr->value, dfi[j].value);
			strcpy(addr->unit, dfi[j].unit);

			addr->slot = -1;
			addr->page = 0;

			++mi;
		}
	}

	metric_make_empty(&_metrics[mi]);
}
