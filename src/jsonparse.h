#ifndef JSONPARSE_H
#define JSONPARSE_H

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

	struct datafield {
		char name[32];
		char value[32];
		char time[32];
		char unit[32];
	};

	void initializeData(const char* data);

	int numDataFields();

	void clearData();

	struct datafield* getDataDump(struct datafield*);

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #ifndef JSONPARSE_H */
