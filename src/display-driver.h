#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */
	struct borderwidth {
		int top;
		int left;
		int right;
		int bottom;
	};

	struct windim {
		int cols;
		int rows;
		int pages;
	};

/** Structure of a data metric
 *
 * Data metrics consist of a name, value, and unit triplet. These are
 * intended to be passed to the update routine as an array. A zero-ed
 * out metric will describe the end of the array.
 *
 * @param page The form page the metric will rest on, zero-indexed
 *
 * @param slot The zero-indexed metric slot on the page, vertically
 * arranged, the metric will be displayed on. If -1, it will be
 * displayed on the next empty slot on that page. If there is no room
 * on the page or another metric occupies the slot, the metric will
 * not be displayed.
 */
	struct metric {
		char name[36];
		char value[36];
		char unit[36];
		int page;
		int slot;
	};

/** Structure to describe a form holding sensor data metrics
 *
 * Data are organized into three rows: metric name, metric value and
 * the unit of the metric. One of these triplets is refered to as a
 * "metric." Each metric can be assigned to a different page, not
 * exceeding the number of pages defined in the @ref windim
 * struct.
 *
 * The window will be demarcated by a solid outer line (appearance
 * depends somewhat on terminal). An inner line is inset from the outer
 * line depending on the values in the @ref borderwidth struct.
 *
 * @param bw Values demarcating distance between inner and outer
 * boundary boxes
 *
 * @param wd Current values for number of columns displayed, number of
 * rows displayed, and number of pages allocated. The columns and rows
 * are adjusted dynamically, while pages is allocated at
 * initialization of the metric form
 *
 * @param metrics Array of @ref metric objects terminated will a
 * zeroed-out struct.
 *
 * @param polldata_cb Function to be called on each loop of the form
 * driver. The argument (long) represents a timeout in
 * milliseconds. The callback should return -1 on failure, 0 if data
 * was updated, and 1 if function timed out.
 */
	struct metric_form {
		struct borderwidth bw;
		struct windim wd;
		struct metric *metrics;
		int (*polldata_cb)(long);
	};

/** Initialize metric form and run the form on the current terminal
 *
 * The @ref metric_form structure will be initialized and the form
 * loop will run, blocking other activites.
 *
 * @param mf A metric_form object with form configuration. An initial
 * set of values must be supplied in the metrics member of the
 * metric_form, and the polldata_cb must also be supplied.
 *
 * @return Returns 0 if exited normally, non-zero if exited with error
 */
	int metric_form_init(struct metric_form *mf);

/** Height of inner form window
 *
 * Calculates the current height of the inner form window.
 *
 * @param mf The metric form object to operate on
 *
 * @return The integer value of the rows occupied by the
 * inner form window
 */
	unsigned int metric_form_height(struct metric_form *mf);

/** Width of inner form window
 *
 * Calculates the current width of the inner form window.
 *
 * @param mf The metric form object to operate on
 *
 * @return The integer value of the columns occupied by the
 * inner form window
 */
	unsigned int metric_form_width(struct metric_form *mf);

/** Checks if the metric object given is empty
 *
 * @param met Const point to @ref metric object to check for emptiness
 *
 * @return True if metric is empty, false if metric is not
 */
	bool metric_is_empty(const struct metric *met);

/** Sets the contents of each metric structure in an array to empty
 *
 * @param met Pre-allocated pointer to the metric array to modify
 *
 * @param len Length of the given array
 */
	void metric_make_empty_array(struct metric *met, int len);

/** Sets the contents of the given metric structure to empty
 *
 * Zeros out and nulls the members of the given metric structure.
 *
 * @param met A pointer to the metric object to operate on
 */
	void metric_make_empty(struct metric *met);

/** Sets the exit flag in the metric form driver */
	void metric_form_exit();

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #ifndef DISPLAY_DRIVER_H */
