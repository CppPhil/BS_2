#ifndef Header_H
#define Header_H

#include <stdio.h> // printf, fprintf, getchar, putchar, ...
#include <stdlib.h> // EXIT_SUCCESS, NULL, ...
#include <pthread.h> // pthread_create, pthread_cond_t, pthread_mutex_t, ...
#include <sched.h> // SCHED_FIFO, SCHED_RR, ...
#include <stdbool.h> // bool, true, false
#include <iso646.h> // not, and, or, ...
#include <errno.h> // errno, error codes
#include <assert.h> // assert
#include <stddef.h> // size_t

#ifdef _WIN32
#	define NOMINMAX // do not define min and max macros
#	include <Windows.h> // Sleep
static void WINAPI sleep(unsigned timeVal) {
    Sleep(timeVal * 1000UL);
}
#else
#   include <unistd.h> // sleep
#endif

#define NUM_THREADS	4   // number of threads, excluding the main thread
#define PROD_SLEEP_TIME 3U // 3 seconds
#define CONS_SLEEP_TIME 2U // 2 seconds
#define CHARS_IN_ALPHABET   26
#define MAX 16  // amount of elements that can fit into the buffer
#define PRINT_BUFFER_BASE_SIZE   30U

typedef struct { // this type is used to track the status of a thread
    pthread_mutex_t pauseMutex; // this mutex is used to synchronize reads writes to the isPaused bool
    pthread_cond_t pauseCondVar; // this cond_var is used by the control thread to notify a thread about being unpaused
    bool isPaused;
    pthread_mutex_t aliveMutex; // this mutex is used to synchronize reads and writes to the isAlive bool
    bool isAlive;
} ThreadControl;

typedef struct {
    int buffer[MAX]; // buffer that holds the characters written into it by the producer threads
    int *p_in; // writing pointer used by the producer threads to write into the buffer
    int *p_out; // reading pointer used by the consumer thread to read from the buffer
    int count; // count representing the amount of characters still to be read from the buffer
    // the producers will increment this count after having written to the buffer
    // the consumer will decrement this count after having read from the buffer
} RingBuffer;

typedef struct { // this type holds ThreadControl types (see above) for the two producer threads and the consumer thread
    ThreadControl prod1Control;
    ThreadControl prod2Control;
    ThreadControl consControl;
} Status;

typedef struct {
    char *buf;
    size_t size;
} PrintBuffer;

extern pthread_mutex_t rb_mutex; // these global variables are defined in another source file (main.c)
extern RingBuffer * const p_rb; // using extern we make them visible in all source files that include this header.
extern Status status;
extern pthread_cond_t not_empty_condvar;
extern pthread_cond_t not_full_condvar;
extern PrintBuffer consPrintBuffer;

#define P_START	(int *)(p_rb->buffer)   // macros for the beginning and the end of the buffer
#define P_END	(int *)((p_rb->buffer) + MAX - 1)

void *p_1_w(void *pid); // forward declarations for all the functions
void *p_2_w(void *pid);
void *consumer(void *pid);
void *control(void *pid);
void advancePtr(int **ptr);
char writeToBuffer(char baseChar, int offset);
char readFromBuffer(void);
void *exitThread(pthread_mutex_t *pAliveMutex);
void killConsAndProds(void);
bool isBufferFull(void);
bool bufferHasContent(void);
bool isBufferEmpty(void);
bool bufferCanHoldMore(void);
void printStatusChange(char const *formatStr, bool boolExpr,
                       char const *trueText, char const *falseText);
void checkReturnVal(int errorCode, char const *file,
                    char const *callingFunction, long line);
void printBufferConstruct(PrintBuffer *printBuffer, size_t bufSize);
void printBufferEnlarge(PrintBuffer *printBuffer, size_t newBufSize);
void printBufferDestroy(PrintBuffer *printBuffer);

#define CHECK(check_me)  checkReturnVal(check_me, __FILE__, \
                                        __FUNCTION__, __LINE__) // macro to be used to have checkReturnVal check the return value of a function call, along with the filename, functionname and linenumber.
// works for most of the posix functions as they return 0, indicating success, or the error code
#define CHECK_ERRNO(fctCall)    fctCall < 0 ? CHECK(errno) : \
                                              (void)0   // this macro is used for some standard library functions that return a negative number on error and then store the error code in errno

// these macros can handle all errors that functions may issue in this program, except for those issued by getchar.

#endif // Header_H
