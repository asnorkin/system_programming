#pragma once
#include <stdio.h>

#define DUMPFILE_PATH "C:\\TMP\\DllDebugDump.txt"


#define InfoLog(msg, ...) do {									\
						FILE *dumpfile;							\
						fopen_s(&dumpfile, DUMPFILE_PATH, "a"); \
						char buf[1024];							\
						sprintf_s(buf, msg, __VA_ARGS__);		\
						fprintf(dumpfile, "%s\n", buf);			\
						fclose(dumpfile);						\
					} while(0)									

#define InfoLogRet(ret, msg, ...) do {							\
						FILE *dumpfile;							\
						fopen_s(&dumpfile, DUMPFILE_PATH, "a"); \
						char buf[1024];							\
						sprintf_s(buf, msg, __VA_ARGS__);		\
						fprintf(dumpfile, "%s\n", buf);			\
						fclose(dumpfile);						\
						return ret;								\
					} while(0)