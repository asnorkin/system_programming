#pragma once


#define ErrorLog(msg) do {														\
							_tprintf(_T("%s: %d\n"), _T(msg), GetLastError());	\
						} while(0)

#define ErrorLogRet(msg, ret) do {													\
								_tprintf(_T("%s: %d\n"), _T(msg), GetLastError());	\
								return ret;											\
							 } while(0)

#define InfoLog(msg, ...) do {						\
						 printf(msg, __VA_ARGS__);	\
						 printf("\n");				\
			 		 } while(0)

#define InfoLogRet(msg, ret) do {							\
							 _tprintf(_T("%s\n"), _T(msg));	\
							 return ret;					\
			 			  } while(0)

#define ErrorExit(msg)	do {													\
							_tprintf(_T("%s: %d\n"), _T(msg), GetLastError());	\
							return -1;											\
						} while(0)

#define InfoExit(msg)  do {									\
							_tprintf(_T("%s\n"), _T(msg));	\
							return -1;						\
						} while(0)

#define ALIGN_DOWN(x, align)  (x & ~(align - 1))
#define ALIGN_UP(x, align)    ((x & (align - 1)) ? ALIGN_DOWN(x, align) + align : x)
