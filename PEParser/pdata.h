#pragma once

#include <Windows.h>


typedef enum _UNWIND_OP_CODES {
	UWOP_PUSH_NONVOL = 0,		/* info == register number */
	UWOP_ALLOC_LARGE = 1,		/* no info, alloc size in next 2 slots */
	UWOP_ALLOC_SMALL = 2,		/* info == size of allocation / 8 - 1 */
	UWOP_SET_FPREG = 3,			/* no info, FP = RSP + UNWIND_INFO.FPRegOffset*16 */
	UWOP_SAVE_NONVOL = 4,		/* info == register number, offset in next slot */
	UWOP_SAVE_NONVOL_FAR = 5,	/* info == register number, offset in next 2 slots */
	UWOP_SAVE_XMM128 = 8,		/* info == XMM reg number, offset in next slot */
	UWOP_SAVE_XMM128_FAR = 9,	/* info == XMM reg number, offset in next 2 slots */
	UWOP_PUSH_MACHFRAME = 10	/* info == 0: no error-code, 1: error-code */
} UNWIND_CODE_OPS;



typedef union _UNWIND_CODE {
	struct {
		BYTE CodeOffset;
		BYTE OpCode : 4;
		BYTE OpInfo : 4;
	} Prologue;
	struct {
		BYTE OffsetLowOrSize;
		BYTE OpCode : 4;
		BYTE OffsetHighOrFlags : 4;
	} Epilogue;
	WORD FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;



typedef struct _UNWIND_INFO {
	BYTE Version : 3;
	BYTE Flags : 5;
	BYTE SizeOfProlog;
	BYTE CountOfCodes;
	BYTE FrameRegister : 4;
	BYTE FrameOffset : 4;
	UNWIND_CODE UnwindCode[1];
} UNWIND_INFO, *PUNWIND_INFO;


typedef struct _ExceptionHandlerInfo {
	ULONG pExceptionHandler;
	ULONG HandlerData;
	ULONG padding;
} ExceptionHandlerInfo_t;


typedef union _Variable {
	ExceptionHandlerInfo_t ExceptionHandlerInfo;
	IMAGE_IA64_RUNTIME_FUNCTION_ENTRY ChainedUnwindInfo;
} Variable, *PVariable;