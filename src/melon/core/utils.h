#ifndef __melon__utils_h__
#define __melon__utils_h__

#include "melon/core/types.h"
#include <stdlib.h>
#include <stdarg.h>

typedef struct VM VM;
typedef struct Value Value;
typedef struct GCItem GCItem;

struct StrFormat
{
    TSize size;
    TSize used;
    char* buffer;
};

void melStringFmtUtils(struct StrFormat* sf, const char* fmt, ...);
void melStringFmtVargsUtils(struct StrFormat* sf, const char* fmt, va_list args);
void melStringFmtFreeUtils(struct StrFormat* sf);

struct StrFormat melDumpStackUtils(VM* vm);
void melPrintStackUtils(VM* vm);

struct StrFormat melDumpCallStackUtils(VM* vm);
void melPrintCallStackUtils(VM* vm);

struct StrFormat melDumpValueUtils(VM* vm, const Value* val);
void melPrintValueUtils(VM* vm, const Value* val);
void melPrintGCItemUtils(VM* vm, const GCItem* item);

struct StrFormat melDumpUpvaluesUtils(VM* vm);
void melPrintUpvaluesUtils(VM* vm);

struct StrFormat melDumpGCInfoUtils(VM* vm, TBool includeSize);
void melPrintGCInfoUtils(VM* vm, TBool includeSize);

TRet melExtractSourceFragmentUtils(
    struct StrFormat* sf, 
    const char* source, 
    TSize sourceLen, 
    TSize line, 
    TSize col, 
    TSize len, 
    TSize ctxLen
);

void melPrintErrorAtSourceUtils(
    VM* vm,
    const char* type,
    const char* message,
    const char* file,
    const char* source,
    TSize sourceSize,
    TSize line,
    TSize col,
    TSize len,
    TSize ctxLen
);

void melVMPrintFunctionUtils(struct StrFormat* sf, void* ctx);

typedef struct MelTimeHD
{
    TUint64 secs;
    TUint64 nanoSecs;
} MelTimeHD;

void melGetTimeHD(MelTimeHD* out);
void melGetTimeDiffHD(const MelTimeHD* start, const MelTimeHD* end, MelTimeHD* out);
TUint64 melGetTimeDiffNs(const MelTimeHD* start, const MelTimeHD* end);

#endif // __melon__utils_h__