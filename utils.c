#include "Header.h"

void advancePtr(int **ptr) { // take a pointer by reference and advance it
    // we take the address to a pointer, so we dereference it and increment that pointer
    // (or assign P_START to it if it's at the end)
    assert((ptr != NULL and *ptr != NULL) and "invalid pointer "
           "passed to void advancePtr(int const **)");
    if (*ptr == P_END) {
        *ptr = P_START;
    } else {
        ++*ptr;
    }
}

char writeToBuffer(char baseChar, int offset) {
    *p_rb->p_in = baseChar + (char)offset;
    ++p_rb->count;
    int *retPtr = p_rb->p_in;
    advancePtr(&p_rb->p_in);
    return *(char *)retPtr;
}

char readFromBuffer(void) {
    char charRead = *(char *)p_rb->p_out;
    --p_rb->count;
    advancePtr(&p_rb->p_out);
    return charRead;
}

void *exitThread(pthread_mutex_t *pAliveMutex) { // this function effectively "kills" the calling thread.
    CHECK(pthread_mutex_unlock(pAliveMutex)); // unlock the alive mutex first, as it was locked last
    CHECK(pthread_mutex_unlock(&rb_mutex)); // we check wether the thread is alive before mutating the data, or printing anything to the console for that matter, to prevent getting "extra" output after we have issued the shutdown command ('q' or 'Q') to the control thread.
    // at that point we will always have the rb_mutex, so we need to release it
    return (void *)EXIT_SUCCESS; // the calling thread quits returning this functions return value, as this is the only way a thread is supposed to exit EXIT_SUCCESS (0) is returned to indicate success.
}

bool isBufferFull(void) { // MAX represents the amount of characters the buffer can hold
    // count represents the amount of characters still to be read from the buffer
    return p_rb->count >= MAX; // if we have MAX elements still to be read, the buffer is full
}

bool bufferHasContent(void) {
    return p_rb->count > 0; // if the buffer has any characters still to be read it still has "content"
    // note that "content" refers to unread characters, anything else is really just garbage.
}

bool isBufferEmpty(void) {
    return p_rb->count <= 0; // if there are no characters to read the buffer is regarded "empty"
    // again we refer to the amount of actual content that may be read
}

bool bufferCanHoldMore(void) {
    return p_rb->count < MAX; // if there are less then MAX characters still to be read from the buffer
    // we can still put in some more, as it's not full.
}

void killConsAndProds(void) {
    CHECK(pthread_mutex_lock(&status.prod1Control.pauseMutex));
    status.prod1Control.isPaused = false; // unpause all threads
    CHECK(pthread_mutex_unlock(&status.prod1Control.pauseMutex));
    CHECK(pthread_mutex_lock(&status.prod2Control.pauseMutex));
    status.prod2Control.isPaused = false;
    CHECK(pthread_mutex_unlock(&status.prod2Control.pauseMutex));
    CHECK(pthread_mutex_lock(&status.consControl.pauseMutex));
    status.consControl.isPaused = false;
    CHECK(pthread_mutex_unlock(&status.consControl.pauseMutex));
    CHECK(pthread_cond_signal(&status.prod1Control.pauseCondVar)); // notify them about being unpaused
    CHECK(pthread_cond_signal(&status.prod2Control.pauseCondVar));
    CHECK(pthread_cond_signal(&status.consControl.pauseCondVar));
    CHECK(pthread_mutex_lock(&status.prod1Control.aliveMutex));
    status.prod1Control.isAlive = false; // let all threads go kill themselves
    CHECK(pthread_mutex_unlock(&status.prod1Control.aliveMutex));
    CHECK(pthread_mutex_lock(&status.prod2Control.aliveMutex));
    status.prod2Control.isAlive = false;
    CHECK(pthread_mutex_unlock(&status.prod2Control.aliveMutex));
    CHECK(pthread_mutex_lock(&status.consControl.aliveMutex));
    status.consControl.isAlive = false;
    CHECK(pthread_mutex_unlock(&status.consControl.aliveMutex));
    p_rb->count = MAX / 2; // set the count of elements to be read to something that neither indicates emptyness nor fullness, so that the threads don't go back waiting for the condvar again
    CHECK(pthread_cond_signal(&not_empty_condvar)); // make the threads wake up if they're waiting for the buffer to change
    CHECK(pthread_cond_broadcast(&not_full_condvar));
}

void printStatusChange(char const *formatStr, bool boolExpr, // this function is used to print information about threads being paused or unpaused
                       char const *trueText, char const *falseText) {
    assert((formatStr != NULL and trueText != NULL
           and falseText != NULL) and "invalid input for "
           "void printStatusChange(char const *, bool,"
           " char const *, char const *");
    CHECK_ERRNO(printf(formatStr, boolExpr ? trueText : falseText));
}

#define PRINT_ERR(string)   fprintf(stderr, string) // we don't check errno for these as that could (potentially) cause infinite recursion

void checkReturnVal(int errorCode, char const *file, // this function is used by the CHECK and CHECK_ERRNO macros to print error codes with line numbers, file and the calling function of where the error occured
                    char const *callingFunction, long line) {
    assert((file != NULL and callingFunction != NULL)
           and "invalid input for "
           "void checkReturnVal(int, char const *,"
           " char const *, long)");
    static char const errorMsg[] = "Error code: %d\n\n";
    switch (errorCode) {
        case 0 : // 0 indicates no error.
            return; // so get outta here
        case ENOMEM :
            PRINT_ERR("ENOMEM");
            break;
        case EINVAL :
            PRINT_ERR("EINVAL");
            break;
        case ENOTSUP :
            PRINT_ERR("ENOTSUP");
            break;
        case ENOSYS :
            PRINT_ERR("ENOSYS");
            break;
        case EAGAIN :
            PRINT_ERR("EAGAIN");
            break;
        case EPERM :
            PRINT_ERR("EPERM");
            break;
        case EINTR :
            PRINT_ERR("EINTR");
            break;
        case ESRCH :
            PRINT_ERR("ESRCH");
            break;
        case EDEADLK :
            PRINT_ERR("EDEADLK");
            break;
        case EBUSY :
            PRINT_ERR("EBUSY");
            break;
        case ETIMEDOUT :
            PRINT_ERR("ETIMEDOUT");
            break;
        default :
            PRINT_ERR("An error");
            break;
    }
    PRINT_ERR(" occured!\n");
    fprintf(stderr, "File: %s\nCalling function: %s\n"
            "Line: %ld\n", file,
            callingFunction, line);
    fprintf(stderr, errorMsg, errorCode);
}
#undef PRINT_ERR

void printBufferConstruct(PrintBuffer *printBuffer, size_t bufSize) {
    printBuffer->buf = malloc(bufSize * sizeof(char));
    assert((printBuffer->buf != NULL) && "malloc failed in printBufferConstruct");
    printBuffer->size = bufSize;
}

void printBufferEnlarge(PrintBuffer *printBuffer, size_t newBufSize) {
    char *newBuf = realloc(printBuffer->buf, newBufSize * sizeof(char));
    assert((newBuf != NULL) && "realloc failed in printBufferEnlarge");
    printBuffer->buf = newBuf;
    printBuffer->size = newBufSize;
}

void printBufferDestroy(PrintBuffer *printBuffer) {
    free(printBuffer->buf);
    printBuffer->size = 0U;
}
