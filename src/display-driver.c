#include "display-driver.h"

#include <form.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#ifndef VM_VERSION
#define VM_VERSION "Unknown"
#endif /* #ifndef VM_VERSION */

#define DISPLAY_MAX_COLS 80
#define DISPLAY_MIN_COLS 80

#define DISPLAY_MAX_ROWS 200
#define DISPLAY_MIN_ROWS 23

/* Flags */
#define METRIC_FLAG_WINRESIZE 0x01
#define METRIC_FLAG_EXIT 0x02

static uint8_t _metric_flags = 0;
static FIELD** fields = NULL;
static FIELD** names = NULL;
static FIELD** values = NULL;
static FIELD** units = NULL;
static FORM* form = NULL;
static WINDOW* win_form = NULL;
static WINDOW* win_main = NULL;
static uint8_t ignore_poll_error = 0;

/* Local function definitions */
static void _update_fields(struct metric_form *mf);
static int _assign_form_to_win(struct metric_form *mf);
static void _allocate_fields(struct metric_form *mf);
static void _define_win_size(struct metric_form *mf);
static void _resize_window(struct metric_form *mf);
static void _handle_winch(int sig);
static void _free_fields();
static unsigned int _fields_per_page(struct metric_form *mf);
static void _form_setup_window();
static void _last_updated_time(struct metric_form *mf);
static void _form_exit();

/*
**********************************************************************
********************* API IMPLEMENTATION *****************************
**********************************************************************
*/

int metric_form_init(struct metric_form *mf)
{
	assert(mf);

	signal(SIGWINCH, _handle_winch);

	initscr();

	_define_win_size(mf);

	assert(win_main);
	assert(win_form);

	_allocate_fields(mf);
	_update_fields(mf);
	form = new_form(fields);
	assert(form);

	if (_assign_form_to_win(mf) != 0) {
		return 1;
	}

	_form_setup_window();

	post_form(form);
	_last_updated_time(mf);
	refresh();
	wrefresh(win_main);
	wrefresh(win_form);

	for (;;) {
		int ret;

		if (_metric_flags & METRIC_FLAG_WINRESIZE) {
			_resize_window(mf);
			_metric_flags &= ~METRIC_FLAG_WINRESIZE;
		}

		/* Exit if receive signal */
		if (_metric_flags & METRIC_FLAG_EXIT) {
			_form_exit();
			return 0;
		}

		ret = mf->polldata_cb(500); /* 500 ms timout on user poll */

		if (ret < 0) {
			_form_exit();
			return 1;
		}

		if (ret == 0) {
			_update_fields(mf);
			_last_updated_time(mf);
			refresh();
			wrefresh(win_main);
			wrefresh(win_form);
		}
	}
}

unsigned int metric_form_height(struct metric_form *mf)
{
	return mf->wd.rows - mf->bw.top - mf->bw.bottom - 2;
}

unsigned int metric_form_width(struct metric_form *mf)
{
	return mf->wd.cols - mf->bw.left - mf->bw.right - 2;
}

void metric_form_exit()
{
	_metric_flags |= METRIC_FLAG_EXIT;
}

bool metric_is_empty(const struct metric *met)
{
	assert(met);

	return strlen(met->name) == 0 &&
		strlen(met->value) == 0 &&
		strlen(met->unit) == 0 &&
		met->page == 0 &&
		met->slot == 0;
}

void metric_make_empty_array(struct metric *met, int len)
{
	assert(met);

	for (int i = 0; i < len; ++i) {
		metric_make_empty(&met[i]);
	}
}

void metric_make_empty(struct metric *met)
{
	assert(met);

	met->name[0] = '\0';
	met->value[0] = '\0';
	met->unit[0] = '\0';
	met->page = 0;
	met->slot = 0;
}

void metric_emerg_exit()
{
	_form_exit();
}

/*
**********************************************************************
***************** LOCAL FUNCTION IMPLEMENTATION **********************
**********************************************************************
*/

static unsigned int _fields_per_page(struct metric_form *mf)
{
	return metric_form_height(mf) / 2;
}

static void _last_updated_time(struct metric_form *mf)
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
		mvwprintw(win_main, mf->wd.rows - 2, 2,
			  "Last update: %s", output);
}

static void _update_fields(struct metric_form *mf)
{
	assert(mf->metrics);

	for (int i = 0; i < mf->wd.pages; ++i) {
		int pfields = _fields_per_page(mf);
		struct metric ms[pfields];
		struct metric mauto[pfields];
		int ai = 0;

		metric_make_empty_array(ms, pfields);
		metric_make_empty_array(mauto, pfields);

		/* Need to sort out the minuses */
		for (int j = 0; !metric_is_empty(&mf->metrics[j]); ++j) {
			if (mf->metrics[j].page != i)
				continue;

			if (mf->metrics[j].slot < 0 &&
			    ai < pfields) {
				mauto[ai] = mf->metrics[j];
				++ai;
			} else {
				int addr = mf->metrics[j].slot;

				if (addr >= pfields) {
					continue;
				}

				if (!metric_is_empty(&ms[addr])) {
					continue;
				}

				ms[addr] = mf->metrics[j];
			}
		}

		/* Sort minuses into empty slots */
		for (int j = 0; j < ai; ++j) {
			for (int k = 0; k < pfields; ++k) {
				if (metric_is_empty(&ms[k])) {
					ms[k] = mauto[j];

					break;
				}
			}
		}

		/* Write all fields on this page */
		for (int j = 0; j < pfields; ++j) {
			int paddr = j + i * pfields;

			set_field_buffer(names[paddr], 0, ms[j].name);
			set_field_buffer(values[paddr], 0, ms[j].value);
			set_field_buffer(units[paddr], 0, ms[j].unit);
		}
	}
}

void _allocate_fields(struct metric_form *mf)
{
	int nfields = _fields_per_page(mf);
	int npages = mf->wd.pages;

	names = (FIELD**) malloc(nfields * npages * sizeof(FIELD*));
	values = (FIELD**) malloc(nfields * npages * sizeof(FIELD*));
	units = (FIELD**) malloc(nfields * npages * sizeof(FIELD*));
	fields = (FIELD**) malloc((nfields * npages * 3 + 1) * sizeof(FIELD*));

	int fieldsctr = 0;
	int pagesctr = 0;

	for (int i = 0; i < nfields * npages; ++i) {
		int row_coord = (i - (pagesctr * nfields)) * 2;
		bool newpage = pagesctr && (! i % nfields);

		names[i] = new_field(1, 15, row_coord, 2, 0, 0);
		values[i] = new_field(1, 15, row_coord, 17, 0, 0);
		units[i] = new_field(1, 10, row_coord, 34, 0, 0);

		field_opts_off(names[i], O_ACTIVE);
		field_opts_off(values[i], O_ACTIVE);
		field_opts_off(units[i], O_ACTIVE);

		/* Signal start of new page on names only if
		   appropriate */
		set_new_page(names[i], newpage);

		set_field_just(values[i], JUSTIFY_RIGHT);

		fields[fieldsctr] = names[i]; fieldsctr++;
		fields[fieldsctr] = values[i]; fieldsctr++;
		fields[fieldsctr] = units[i]; fieldsctr++;

		if (! (i + 1) % nfields)
			++pagesctr;
	}

	fields[fieldsctr] = NULL;
}

static void _form_setup_window()
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

static void _resize_window(struct metric_form *mf)
{
	endwin();
	refresh();
	clear();

	/* Delete old window and create new one */
	delwin(win_form);
	delwin(win_main);
	_define_win_size(mf);
	_free_fields();

	/* Adjust form overflow */
	_allocate_fields(mf);
	assert(fields);
	_update_fields(mf);

	form = new_form(fields);
	_assign_form_to_win(mf);

	_form_setup_window();

	assert(form);
	post_form(form);

	refresh();
	wrefresh(win_main);
	wrefresh(win_form);
}

static void _free_fields()
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

static void _form_exit()
{
	_free_fields();
	endwin();
}

void _define_win_size(struct metric_form *mf)
{
	/* Set border width */
	mf->bw.left = 2;
	mf->bw.right = 2;
	mf->bw.top = 2;
	mf->bw.bottom = 2;

	/* Set window frame size */
	mf->wd.cols = COLS < DISPLAY_MAX_COLS
		? COLS : DISPLAY_MAX_COLS;
	mf->wd.cols = mf->wd.cols > DISPLAY_MIN_COLS
		? mf->wd.cols : DISPLAY_MIN_COLS;

	mf->wd.rows = LINES < DISPLAY_MAX_ROWS
		? LINES : DISPLAY_MAX_ROWS;
	mf->wd.rows = mf->wd.cols > DISPLAY_MIN_ROWS
		? mf->wd.rows : DISPLAY_MIN_ROWS;

	win_main = newwin(mf->wd.rows, /* Lines */
			  mf->wd.cols, /* Cols */
			  0, /* X */
			  0); /* Y */
	win_form = derwin(win_main,
			  mf->wd.rows - mf->bw.left - mf->bw.right, /* Lines */
			  mf->wd.cols - mf->bw.top - mf->bw.bottom, /* Cols */
			  mf->bw.left, /* X */
			  mf->bw.top); /* Y */
}

static int _assign_form_to_win(struct metric_form *mf)
{
	if (set_form_win(form, win_main) != 0) {
		perror("Critical Error setting main window: ");
		return 1;
	}

	if (set_form_sub(form,
			 derwin(win_form, metric_form_height(mf),
				metric_form_width(mf), 1, 1)) != 0) {
		perror("Critical Error setting sub window: ");
		return 1;
	}

	return 0;
}

static void _handle_winch(int sig)
{
	assert(sig == SIGWINCH);
	ignore_poll_error = 1;
	_metric_flags |= METRIC_FLAG_WINRESIZE;
}
