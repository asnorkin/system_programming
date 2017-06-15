#pragma once

#define InfoLog(msg, ...) do {								\
						char buf[1024];						\
						sprintf_s(buf, msg, __VA_ARGS__);	\
						printf("%s\n", buf);				\
					} while(0)								\

#define InfoLogRet(ret, msg, ...) do {						\
						char buf[1024];						\
						sprintf_s(buf, msg, __VA_ARGS__);	\
						printf("%s\n", buf);				\
						return ret;							\
					} while(0)

#define ErrorLog(msg, ...) do {									\
						char buf[1024];							\
						sprintf_s(buf, msg, __VA_ARGS__);		\
						printf("%s: %d\n", buf, GetLastError());\
					} while(0)									

#define ErrorLogRet(ret, msg, ...) do {							\
						char buf[1024];							\
						sprintf_s(buf, msg, __VA_ARGS__);		\
						printf("%s: %d\n", buf, GetLastError());\
						return ret;								\
					} while(0)									