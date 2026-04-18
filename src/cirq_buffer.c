#include "cirq_buffer.h"
#include <string.h>

void CirqBuff_Init(CirqBufferType *cirqbuffer, void *buffer, int maxCnt, size_t dataSize) {
    cirqbuffer->maxCnt = maxCnt;
    cirqbuffer->currCnt = 0;
    cirqbuffer->dataSize = dataSize;
    cirqbuffer->head = buffer;
    cirqbuffer->tail = buffer;
    cirqbuffer->bufStart = buffer;
    cirqbuffer->bufEnd = (char*)buffer + (maxCnt * dataSize);
}

int CirqBuffPush( CirqBufferType *cPtr, void *dataPtr ) {
    if (cPtr->currCnt >= cPtr->maxCnt) return -1;
    memcpy(cPtr->tail, dataPtr, cPtr->dataSize);
    cPtr->tail = (char*)cPtr->tail + cPtr->dataSize;
    if (cPtr->tail >= cPtr->bufEnd) cPtr->tail = cPtr->bufStart;
    ++cPtr->currCnt;
    return 0;
}

int CirqBuffPop(CirqBufferType *cPtr, void *dataPtr) {
    if (cPtr->currCnt == 0) return -1;
    memcpy(dataPtr, cPtr->head, cPtr->dataSize);
    cPtr->head = (char*)cPtr->head + cPtr->dataSize;
    if (cPtr->head >= cPtr->bufEnd) cPtr->head = cPtr->bufStart;
    --cPtr->currCnt;
    return 0;
}

void *CirqBuff_PushLock( CirqBufferType *cPtr){
    if (cPtr->currCnt >= cPtr->maxCnt) return NULL;
    return cPtr->tail;
}

void *CirqBuff_PopLock(CirqBufferType *cPtr ){
    if (cPtr->currCnt == 0) return NULL;
    return cPtr->head;
}


CirqBufferType *CirqBuffDynCreate( size_t size, size_t dataSize ) {
    (void)size; (void)dataSize;
    return NULL;
}

int CirqBuffDynDestroy(CirqBufferType *cPtr ){
    (void)cPtr; return 0;
}

CirqBufferType CirqBuffStatCreate(void *buffer, int maxCnt, size_t dataSize) {
    CirqBufferType c;
    CirqBuff_Init(&c, buffer, maxCnt, dataSize);
    return c;
}
