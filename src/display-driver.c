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

#define DISPLAY_MAX_COLS 80
#define DISPLAY_MIN_COLS 80

#define DISPLAY_MAX_ROWS 200
#define DISPLAY_MIN_ROWS 23

struct borderwidth {
	int top;
	int left;
	int right;
	int bottom;
} bw;

struct windim {
	int cols;
	int rows;
} wd;

static int displayFD = -1;
static FIELD** fields = NULL;
static FIELD** names = NULL;
static FIELD** values = NULL;
static FIELD** units = NULL;
static unsigned int nfields = 0;
static FORM* form = NULL;
static WINDOW* win_form = NULL;
static WINDOW* win_main = NULL;
static uint8_t ignore_poll_error = 0;
static int assignFormToWin();

/* Sizes */
static unsigned int form_height();
static void defineWindowSize();
static void formResizeWindow();
static void formHandleWinch(int sig);
static void freeFields();

static void updateFieldValues(struct datafield **df);

static unsigned int form_height() {
	return wd.rows - bw.top - bw.bottom - 2;
}

static unsigned int form_width() {
	return wd.cols - bw.left - bw.right - 2;
}

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
		mvwprintw(win_main, wd.rows - 2, 2,
			  "Last update: %s", output);
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
	lastUpdatedTime();

	initializeData(buff);
	return getDataDump(df);
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

	struct datafield** dfields = NULL;

	dfields = parseData(pdfd, dfields);

	assert(dfields);

	nfields = 0;

	for (size_t i = 0; i < numSensors(); ++i) {
		nfields += numDataFields(i);
	}

	nfields = nfields < form_height() / 2 ? nfields : form_height() / 2;

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

	/* Delete old window and create new one */
	freeFields();
	delwin(win_form);
	delwin(win_main);
	defineWindowSize();

	/* Get new form data */
	popFields(displayFD);
	assert(fields);
	form = new_form(fields);
	assignFormToWin();

	formSetupWindow();

	assert(form);

	post_form(form);

	refresh();
	wrefresh(win_main);
	wrefresh(win_form);
}

static void freeFields()
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

	free(names);
	free(values);
	free(units);
	free(fields);

	names = NULL;
	values = NULL;
	units = NULL;
	fields = NULL;
}

void formExit()
{
	freeFields();
	endwin();
}

void defineWindowSize()
{
	/* Set border width */
	bw.left = 2;
	bw.right = 2;
	bw.top = 2;
	bw.bottom = 2;

	/* Set window frame size */
	wd.cols = COLS < DISPLAY_MAX_COLS ? COLS : DISPLAY_MAX_COLS;
	wd.cols = wd.cols > DISPLAY_MIN_COLS ? wd.cols : DISPLAY_MIN_COLS;

	wd.rows = LINES < DISPLAY_MAX_ROWS ? LINES : DISPLAY_MAX_ROWS;
	wd.rows = wd.cols > DISPLAY_MIN_ROWS ? wd.rows : DISPLAY_MIN_ROWS;

	win_main = newwin(wd.rows, /* Lines */
			  wd.cols, /* Cols */
			  0, /* X */
			  0); /* Y */
	win_form = derwin(win_main,
			  wd.rows - bw.left - bw.right, /* Lines */
			  wd.cols - bw.top - bw.bottom, /* Cols */
			  bw.left, /* X */
			  bw.top); /* Y */
}

static int assignFormToWin()
{
	if (set_form_win(form, win_main) != 0) {
		perror("Critical Error setting main window: ");
		return 1;
	}

	if (set_form_sub(form, derwin(win_form, form_height(),
				      form_width(), 1, 1)) != 0) {
		perror("Critical Error setting sub window: ");
		return 1;
	}

	return 0;
}

int formRun(int pdfd)
{
	displayFD = pdfd;
	signal(SIGWINCH, formHandleWinch);

	initscr();

	defineWindowSize();

	assert(win_main);
	assert(win_form);

	popFields(pdfd);
	form = new_form(fields);
	assert(form);

	if (assignFormToWin() != 0) {
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

		struct datafield **df = NULL;

		df = parseData(pfd.fd, df);
		updateFieldValues(df);
		clearData();
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
