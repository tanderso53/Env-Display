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

#ifndef VM_VERSION
#define VM_VERSION "Unknown"
#endif /* #ifndef VM_VERSION */

#ifndef DISPLAY_DRIVER_INPUT_BUFFER_LEN
#define DISPLAY_DRIVER_INPUT_BUFFER_LEN 4096
#endif

static FIELD** fields = NULL;
static FIELD** names = NULL;
static FIELD** values = NULL;
static FIELD** units = NULL;
static unsigned int nfields = 0;
static FORM* form = NULL;
static WINDOW* win_form = NULL;
static WINDOW* win_main = NULL;
static uint8_t ignore_poll_error = 0;

/* Sizes */
static unsigned int form_height = 24;

static void formResizeWindow();
static void formHandleWinch(int sig);

static void updateFieldValues(struct datafield **df);

void lastUpdatedTime()
{
	char output[32];
	struct tm timstruct;
	time_t curtime = time(NULL);

	/* Load current time into buffer */
	localtime_r(&curtime, &timstruct);
	snprintf(output, 32, "%02d/%02d/%04d %02d:%02d:%02d %s",
		 timstruct.tm_mon + 1, timstruct.tm_mday,
		 timstruct.tm_year + 1900, timstruct.tm_hour,
		 timstruct.tm_min, timstruct.tm_sec,
		 timstruct.tm_zone);

	/* Print current time to bottom of screen */
	if (win_main)
		mvwprintw(win_main, 28, 2, "Last update: %s", output);
}

static void updateFieldValues(struct datafield **df)
{
	unsigned int fieldcnt = 0;

	for (size_t i = 0; i < numSensors(); ++i) {
		struct datafield *dfi = df[i];

		for (int j = 0; j < numDataFields(i); ++j) {
			set_field_buffer(names[fieldcnt], 0, dfi[j].name);
			set_field_buffer(values[fieldcnt], 0, dfi[j].value);
			set_field_buffer(units[fieldcnt], 0, dfi[j].unit);

			++fieldcnt;

			if (fieldcnt == nfields) {
				break;
			}
		}
	}
}

void parseData(int pdfd)
{
	char buff[DISPLAY_DRIVER_INPUT_BUFFER_LEN];

	int readresult = read(pdfd, buff,
			      DISPLAY_DRIVER_INPUT_BUFFER_LEN - 1);

	if (readresult < 0) {
		perror("Error reading file: ");
		raise(SIGINT);
	}
	else if (readresult == 0) {
		sleep(1);
		return;
	}

	buff[readresult] = '\0';
	lastUpdatedTime();

	initializeData(buff);
	struct datafield** df = NULL;
	df = getDataDump(df);

	updateFieldValues(df);

	clearData();
}

void popFields(int pdfd)
{
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

	char buff[DISPLAY_DRIVER_INPUT_BUFFER_LEN];
	struct datafield** dfields = NULL;

	int readresult = read(pfd.fd, buff,
			      DISPLAY_DRIVER_INPUT_BUFFER_LEN - 1);

	if (readresult < 0) {
		perror("Error reading file: ");
		raise(SIGINT);
	}

	buff[readresult] = '\0';
	lastUpdatedTime();

	initializeData(buff);
	nfields = 0;
	dfields = getDataDump(dfields);

	for (size_t i = 0; i < numSensors(); ++i) {
		nfields += numDataFields(i);
	}

	nfields = nfields < form_height / 2 ? nfields : form_height / 2;

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

		field_opts_off(names[i], O_ACTIVE);
		field_opts_off(values[i], O_ACTIVE);
		field_opts_off(units[i], O_ACTIVE);

		set_field_just(values[i], JUSTIFY_RIGHT);

		fields[fieldsctr] = names[i]; fieldsctr++;
		fields[fieldsctr] = values[i]; fieldsctr++;
		fields[fieldsctr] = units[i]; fieldsctr++;
	}

	updateFieldValues(dfields);

	fields[fieldsctr] = NULL;
	clearData();
	/* close(pfd.fd); */
}

static void formSetupWindow()
{
	assert(win_main);
	assert(win_form);

	box(win_main, 0, 0);
	box(win_form, 0, 0);

	/* Title Header */
	mvwprintw(win_main, 1, 2, "Environmental Data Display Program,"
		  " Version: %s",
		VM_VERSION);

	/* Set cursor to invisible if supported */
	curs_set(0);
}

void formDriver(int ch)
{
	switch (ch) {

	default:
		break;

	}
}

static void formResizeWindow()
{
	endwin();
	refresh();
	clear();

	formSetupWindow();

	assert(form);

	refresh();
	wrefresh(win_main);
	wrefresh(win_form);
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

int formRun(int pdfd)
{
	signal(SIGWINCH, formHandleWinch);

	initscr();
	win_main = newwin(30, /* Lines */
			  80, /* Cols */
			  0, /* X */
			  0); /* Y */
	win_form = derwin(win_main,
			  26, /* Lines */
			  76, /* Cols */
			  2, /* X */
			  2); /* Y */

	assert(win_main);
	assert(win_form);

	popFields(pdfd);
	form = new_form(fields);
	assert(form);

	if (set_form_win(form, win_main) != 0) {
		perror("Critical Error setting main window: ");
		return 1;
	}

	if (set_form_sub(form, derwin(win_form, form_height, 74, 1, 1)) != 0) {
		perror("Critical Error setting sub window: ");
		return 1;
	}

	formSetupWindow();

	post_form(form);
	refresh();
	wrefresh(win_main);
	wrefresh(win_form);

	for (;;) {
		struct pollfd pfd = {
			.fd = pdfd, /* open(openfile, O_RDWR), */
			.events = POLLIN
		};

		int pollresult = poll(&pfd, 1, 30000);

		if (pollresult < 0) {

			/* Dirty hack to prevent poll errors on window
			 * size changes */
			if (ignore_poll_error) {
				ignore_poll_error = 0;
				continue;
			}

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
		/* close(pfd.fd); */
	}
}

static void formHandleWinch(int sig)
{
	assert(sig == SIGWINCH);
	ignore_poll_error = 1;
	formResizeWindow();
}
