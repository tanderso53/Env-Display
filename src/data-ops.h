#ifndef DATA_OPS_H
#define DATA_OPS_H

#include "jsonparse.h"
#include "display-driver.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

	extern struct datafield errordf[1];

	struct datafield **pollData(struct datafield **df, uint32_t ms);

	struct datafield **parseData(int pdfd, struct datafield** df);

	bool isErrorDatafield(const struct datafield *df);

	int ncursesPollCB(long mstimeout);

	struct metric_form *ncursesCFG(int pdfd);

	void ncursesFreeMetric();

	void ncursesEmergExit();

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #ifndef DATA_OPS_H */
