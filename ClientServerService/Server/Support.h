#pragma once
#define WBSIZE 1024

#define InfoLog(msg, ...) do {						\
						printf(msg, __VA_ARGS__);	\
					 } while(0)

#define InfoLogRet(ret, msg, ...) do {						\
								printf(msg, __VA_ARGS__);	\
								return ret;				    \
							 } while(0)

#define InfoExit(msg, ...) do {						\
						  printf(msg, __VA_ARGS__);	\
						  exit(EXIT_FAILURE);		\
					  } while(0)

#define ErrorLog(msg, ...)   do {									\
							char buf[WBSIZE];						\
							sprintf_s(buf, msg, __VA_ARGS__);		\
							printf("%s: %d\n", buf, GetLastError());\
						} while(0);

#define ErrorLogRet(ret, msg, ...) do {													\
								char buf[WBSIZE];						\
								sprintf_s(buf, msg, __VA_ARGS__);		\
								printf("%s: %d\n", buf, GetLastError());\
								return ret;								\
							  } while(0);

#define ErrorExit(msg, ...)  do {									\
							char buf[WBSIZE];						\
							sprintf_s(buf, msg, __VA_ARGS__);		\
							printf("%s: %d\n", buf, GetLastError());\
							exit(EXIT_FAILURE);						\
					    } while(0)

#define WSAErrorExit(msg, ...) do {														\
							char buf[WBSIZE];							\
							sprintf_s(buf, msg, __VA_ARGS__);			\
							printf("%s: %d\n", buf, WSAGetLastError());	\
							exit(EXIT_FAILURE);							\
					    } while(0)

#define WSAErrorLog(msg, ...)  do {										\
							char buf[WBSIZE];							\
							sprintf_s(buf, msg, __VA_ARGS__);			\
							printf("%s: %d\n", buf, WSAGetLastError());	\
					    } while(0)

#define WSAErrorLogRet(ret, msg, ...)  do {								\
							char buf[WBSIZE];							\
							sprintf_s(buf, msg, __VA_ARGS__);			\
							printf("%s: %d\n", buf, WSAGetLastError());	\
							return ret;									\
					    } while(0)

#define WSACleanupInfoExit(msg, ...) do {				\
							  WSACleanup();				\
							  printf(msg, __VA_ARGS__);	\
							  exit(EXIT_FAILURE);		\
					      } while(0) 


#define WSACleanupErrorExit(msg, ...) do {									\
								WSACleanup();								\
								char buf[WBSIZE];							\
								sprintf_s(buf, msg, __VA_ARGS__);			\
								printf("%s: %d\n", buf, WSAGetLastError());	\
								exit(EXIT_FAILURE);							\
					      } while(0) 

#define WSACleanupErrorLogRet(ret, msg, ...) do {						\
							WSACleanup();								\
							char buf[WBSIZE];							\
							sprintf_s(buf, msg, __VA_ARGS__);			\
							printf("%s: %d\n", buf, WSAGetLastError());	\
							return ret;									\
						} while (0)
