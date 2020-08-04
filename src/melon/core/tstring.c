#include "melon/core/tstring.h"
#include "melon/core/value.h"
#include "melon/core/vm.h"
#include "melon/core/memory_utils.h"
#include "melon/core/strings_table.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef MELON_STRINGS_TABLE_MAX_CACHE_LEN
#define MELON_STRINGS_TABLE_MAX_CACHE_LEN 40
#endif

#ifdef _TRACK_ALLOCATIONS_GC
#include <stdio.h>
#endif

uint64_t siphash24(const void *src, unsigned long src_sz, const char key[16]);

GCItem* melNewDataString(VM* vm, TSize size)
{
    TSize objSize = sizeof(String) + size;

    GCItem* obj = melNewGCItem(vm, objSize);
    obj->type = MELON_TYPE_STRING;

    String* strObj = melM_strFromObj(obj);
    
#ifdef _TRACK_ALLOCATIONS_GC
    printf("Allocated string of size %llu (%p), total bytes allocated = %llu\n", objSize + sizeof(GCItem), obj, vm->gc.usedBytes);
#endif

    if (size == 0)
    {
        strObj->internalized = 1;
        return obj;
    }
    
    strObj->internalized = 0;
    strObj->len = size - 1;
    strObj->string = (char*)((TPtr)(obj) + sizeof(GCItem) + sizeof(String));

    return obj;
}

TRet melUpdateStringHash(VM* vm, GCItem* s)
{
    String* strObj = melM_strFromObj(s);
    const char* str = melM_strDataFromObj(s);
    
    strObj->hash = siphash24(
        str, 
        strObj->len > MELON_VM_MAX_STRING_HASH_LEN ? MELON_VM_MAX_STRING_HASH_LEN : strObj->len,
        vm->hashKey
    );

    return 0;
}

GCItem* melNewString(VM* vm, const char* str, TSize size)
{
    GCItem* obj = NULL;

    if (size <= MELON_STRINGS_TABLE_MAX_CACHE_LEN)
    {
        obj = melNewDataString(vm, 0);
        String* strObj = melM_strFromObj(obj);
        strObj->len = size;
        strObj->hash = siphash24(str, strObj->len, vm->hashKey);
        strObj->string = melAddStringStringsTable(&vm->stringsTable, str, size, strObj->hash);
        assert(strObj->string != NULL);
    }
    else
    {
        obj = melNewDataString(vm, size + 1);
        
        char* strStart = melM_strDataFromObj(obj);
        memcpy(strStart, str, size);
        strStart[size] = '\0';

        melUpdateStringHash(vm, obj);
    }

    return obj;
}

GCItem* melNewStringFromStrings(VM* vm, GCItem* s1, GCItem* s2)
{
    if (s1->type != MELON_TYPE_STRING || s2->type != MELON_TYPE_STRING)
    {
        melM_fatal(vm, "Can't create string by combining two non-string objects.");
        return NULL;
    }

    String* s1s = melM_strFromObj(s1);
    String* s2s = melM_strFromObj(s2);

    GCItem* newObj = melNewDataString(vm, s1s->len + s2s->len + 1);
    String* newStrObj = melM_strFromObj(newObj);

    char* newStr = melM_strDataFromObj(newObj);
    memcpy(newStr, melM_strDataFromObj(s1), s1s->len);
    memcpy((char *)(newStr + s1s->len), melM_strDataFromObj(s2), s2s->len);
    newStr[newStrObj->len] = '\0';

    melUpdateStringHash(vm, newObj);

#ifdef _TRACK_ALLOCATIONS_GC
    printf("Allocated string of size %llu (%p), total bytes allocated = %llu\n", newStrObj->len + 1 +sizeof(String) + sizeof(GCItem), newObj, vm->gc.usedBytes);
#endif

    return newObj;
}

TRet melFreeString(VM* vm, GCItem* s)
{
    assert(s->type == MELON_TYPE_STRING);

    String* ss = melM_strFromObj(s);
    TSize size = sizeof(String);

    if (ss->internalized)
    {
        melRemoveStringStringsTable(&vm->stringsTable, ss->string, ss->len, ss->hash);
    }
    else
    {
#ifdef _ZERO_MEMORY_ON_FREE_GC
        ss->string[0] = '\0';
#endif
    }

#ifdef _TRACK_ALLOCATIONS_GC
    {
        const char* str = melM_strDataFromObj(s);
        printf("Freeing string of %llu bytes (%p), total bytes now = %llu\n", size + sizeof(GCItem), s, vm->gc.usedBytes - (size + sizeof(GCItem)));
        printf("String is: \"%.*s\"\n", ss->len, str);
    }
#endif  

    vm->gc.usedBytes -= size;

    return melFreeGCItem(vm, s);
}

TRet melCmpStrings(VM* vm, GCItem* s1, GCItem* s2)
{
    assert(s1->type == MELON_TYPE_STRING);
    assert(s2->type == MELON_TYPE_STRING);

    String* s1s = melM_strFromObj(s1);
    String* s2s = melM_strFromObj(s2);

    if (s1s->string == s2s->string)
    {
        return 0;
    }

    char* s1cs = melM_strDataFromObj(s1);
    char* s2cs = melM_strDataFromObj(s2);

    return strcmp(s1cs, s2cs);
}

TRet melFirstIndexOfString(
    VM* vm, 
    GCItem* haystack, 
    GCItem* needle,
    TUint64 start,
    TUint64* index
)
{
    assert(haystack != NULL);
    assert(needle != NULL);
    assert(haystack->type == MELON_TYPE_STRING);
    assert(needle->type == MELON_TYPE_STRING);
    
    String* haystackStr = melM_strFromObj(haystack);

    if (start >= haystackStr->len)
    {
        return 1;
    }

    const char* haystackData = melM_strDataFromObj(haystack);
    String* needleStr = melM_strFromObj(needle);
    const char* needleData = melM_strDataFromObj(needle);

    TInt32 matchLen = 0;

    for (TUint64 i = start; i < haystackStr->len; i++)
    {
        if (haystackData[i] == needleData[matchLen])
        {
            matchLen++;

            if (matchLen == needleStr->len)
            {
                if (index != NULL)
                {
                    *index = i - matchLen + 1;
                }

                return 0;
            }
        }
        else
        {
            matchLen = 0;
        }
    }

    return 1;
}

GCItem* melNewReplaceString(
    VM* vm, 
    GCItem* haystack, 
    GCItem* needle, 
    GCItem* replacement,
    TUint64* firstIdx,
    TUint64* lastIdx
)
{
    assert(haystack != NULL);
    assert(needle != NULL);
    assert(replacement != NULL);
    assert(haystack->type == MELON_TYPE_STRING);
    assert(needle->type == MELON_TYPE_STRING);
    assert(replacement->type == MELON_TYPE_STRING);

    String* haystackStr = melM_strFromObj(haystack);

    TUint64 startIdx = firstIdx != NULL ? *firstIdx : 0;
    TUint64 endIdx = lastIdx != NULL ? *lastIdx : haystackStr->len;

    if (startIdx >= endIdx)
    {
        return haystack;
    }

    const char* haystackData = melM_strDataFromObj(haystack);

    TUint64 matchIndex;

    TUint64* matches = NULL;
    TSize matchesCount = 0;
    TSize matchesCapacity = 0;

    while(melFirstIndexOfString(vm, haystack, needle, startIdx, &matchIndex) == 0)
    {
        if (matchIndex >= endIdx)
        {
            break;
        }

        if (melGrowBuffer((void**)&matches, &matchesCapacity, sizeof(TUint64), ++matchesCount) != 0)
        {
            melM_fatal(vm, "Error resizing string replace matches buffer");
            return NULL;
        }

        matches[matchesCount - 1] = matchIndex;
        startIdx = matchIndex + 1;
    }

    if (matchesCount == 0)
    {
        return haystack;
    }

    String* replacementStr = melM_strFromObj(replacement);
    const char* replacementData = melM_strDataFromObj(replacement);
    String* needleStr = melM_strFromObj(needle);

    TInt64 replDiff = replacementStr->len - needleStr->len;
    TSize newSize = haystackStr->len + replDiff * matchesCount;

    GCItem* newString = melNewDataString(vm, newSize + 1);
    char* newStringData = melM_strDataFromObj(newString);
    TUint64 srcIndex = 0;
    TUint64 dstIndex = 0;

    for (TSize i = 0; i < matchesCount; i++)
    {
        TUint64 index = matches[i];
        TSize len = index - srcIndex;

        memcpy(&newStringData[dstIndex], &haystackData[srcIndex], len);
        dstIndex += len;
        srcIndex += len;
        
        if (replacementStr->len > 0)
        {
            memcpy(&newStringData[dstIndex], replacementData, replacementStr->len);
            dstIndex += replacementStr->len;
        }

        srcIndex += needleStr->len;
    }

    // Copy the reminder of the string
    memcpy(&newStringData[dstIndex], &haystackData[srcIndex], haystackStr->len - srcIndex);
    newStringData[newSize] = '\0';

    melUpdateStringHash(vm, newString);

    free(matches);

    return newString;
}