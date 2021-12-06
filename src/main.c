#include "jsonparse.h"

#include <form.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <time.h>

static FIELD** fields = NULL;
static FIELD** names = NULL;
static FIELD** values = NULL;
static FIELD** units = NULL;
static unsigned int nfields = 0;
static FORM* form = NULL;
static WINDOW* win_form = NULL;
static WINDOW* win_main = NULL;

void lastUpdatedTime()
{
	char output[32];
	struct tm timstruct;
	time_t curtime = time(NULL);

	/* Load current time into buffer */
	localtime_r(&curtime, &timstruct);
	snprintf(output, 32, "%02d/%02d/%04d %02d:%02d:%02d %s",
		 timstruct.tm_mon, timstruct.tm_mday,
		 timstruct.tm_year + 1900, timstruct.tm_hour,
		 timstruct.tm_min, timstruct.tm_sec,
		 timstruct.tm_zone);

	/* Print current time to bottom of screen */
	assert(win_main);
	mvwprintw(win_main, 28, 2, "Last update: %s", output);
}

void parseData(int pdfd)
{
	char buff[1024];

	int readresult = read(pdfd, buff, 1023);

	if (readresult < 0) {
		perror("Error reading file: ");
		raise(SIGINT);
	}

	buff[readresult] = '\0';
	lastUpdatedTime();

	initializeData(buff);
	struct datafield* df = NULL;
	df = getDataDump(df);

	int nlimit;

	if ((unsigned int) numDataFields() < nfields)
		nlimit = numDataFields();
	else
		nlimit = nfields;

	for (int i = 0; i < nlimit; ++i) {
		set_field_buffer(names[i], 0, df[i].name);
		set_field_buffer(values[i], 0, df[i].value);
		set_field_buffer(units[i], 0, df[i].unit);
	}

	clearData();
}

void popFields(const char* parsefile)
{
	struct pollfd pfd = {
		.fd = open(parsefile, O_RDWR),
		.events = POLLIN
	};

	printf("Polling for environmental data...\n");
	int pollresult = poll(&pfd, 1, 60000);

	if (pollresult < 0) {
		perror("Error while reading initial data: ");
		raise(SIGINT);
	}

	if (pollresult == 0) {
		fprintf(fopen("/dev/stderr", "w"), "Failed to get "
			"initial data from stream\n");
		raise(SIGINT);
	}

	char buff[1024];
	struct datafield* dfields = NULL;

	int readresult = read(pfd.fd, buff, 1023);

	if (readresult < 0) {
		perror("Error reading file: ");
		raise(SIGINT);
	}

	buff[readresult] = '\0';
	lastUpdatedTime();

	initializeData(buff);
	nfields = numDataFields();
	dfields = getDataDump(dfields);

	assert(dfields);

	names = (FIELD**) malloc(nfields * sizeof(FIELD*));
	values = (FIELD**) malloc(nfields * sizeof(FIELD*));
	units = (FIELD**) malloc(nfields * sizeof(FIELD*));
	fields = (FIELD**) malloc((nfields * 3 + 1) * sizeof(FIELD*));

	int fieldsctr = 0;

	for (unsigned int i = 0; i < nfields; ++i) {
		names[i] = new_field(1, 15, i * 2, 2, 0, 0);
		values[i] = new_field(1, 15, i * 2, 17, 0, 0);
		units[i] = new_field(1, 10, i * 2, 34, 0, 0);

		set_field_buffer(names[i], 0, dfields[i].name);
		set_field_buffer(values[i], 0, dfields[i].value);
		set_field_buffer(units[i], 0, dfields[i].unit);

		field_opts_off(names[i], O_ACTIVE);
		field_opts_off(values[i], O_ACTIVE);
		field_opts_off(units[i], O_ACTIVE);

		set_field_just(values[i], JUSTIFY_RIGHT);

		fields[fieldsctr] = names[i]; fieldsctr++;
		fields[fieldsctr] = values[i]; fieldsctr++;
		fields[fieldsctr] = units[i]; fieldsctr++;
	}

	fields[fieldsctr] = NULL;
	clearData();
	close(pfd.fd);
}

void formDriver(int ch)
{
	switch (ch) {

	default:
		break;

	}
}

void formExit()
{
	if (form) {
		unpost_form(form);
		free_form(form);
	}

	if (fields) {
		for (int i = 0; fields[i] != NULL; ++i) {
			if (fields[i]) {
				free_field(fields[i]);
			}
		}
	}

	endwin();
}

int formRun(const char* openfile)
{
	initscr();
	win_main = newwin(30, /* Lines */
			  80, /* Cols */
			  0, /* X */
			  0); /* Y */
	box(win_main, 0, 0);
	win_form = derwin(win_main,
			  26, /* Lines */
			  76, /* Cols */
			  2, /* X */
			  2); /* Y */
	box(win_form, 0, 0);

	assert(win_main);
	assert(win_form);

	popFields(openfile);
	form = new_form(fields);
	assert(form);

	if (set_form_win(form, win_main) != 0) {
		perror("Critical Error setting main window: ");
		return 1;
	}

	if (set_form_sub(form, derwin(win_form, 24, 74, 1, 1)) != 0) {
		perror("Critical Error setting sub window: ");
		return 1;
	}

	/* Title Header */
	mvwprintw(win_main, 1, 2, "Environmental Data");

	/* Set cursor to invisible if supported */
	curs_set(0);

	post_form(form);
	refresh();
	wrefresh(win_main);
	wrefresh(win_form);

	for (;;) {
		struct pollfd pfd = {
			.fd = open(openfile, O_RDWR),
			.events = POLLIN
		};

		int pollresult = poll(&pfd, 1, 30000);

		if (pollresult < 0) {
			perror("Critical Error polling file: ");
			formExit();
			exit(1);
		}
		else if (pollresult == 0) {
			continue;
		}

		parseData(pfd.fd);
		refresh();
		wrefresh(win_main);
		wrefresh(win_form);
		close(pfd.fd);
	}
}

void signalHandler(int sig)
{
	formExit();
	printf("Received signal %d\n", sig);
	exit(0);
}

int main(int argc, const char** argv)
{
	char buf[128];

	if (argc > 1) {
		strncpy(buf, argv[1], 128);
	}
	else if (argc > 2) {
		fprintf(stderr, "Error: Too many arguments\n");
		exit(1);
	}
	else {
		strncpy(buf, "/dev/stdin", 128);
	}

	signal(SIGINT, signalHandler);
	return formRun(buf);
}
