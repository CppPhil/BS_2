#include "Header.h"

void *control(void *pid) {
    CHECK_ERRNO(printf("Control thread, PID: %d launched\n",
                       *(int *)pid));
    char const pausedText[] = "paused";
    char const unPausedText[] = "resumed";
    int curInput; // getchar() returns an int
    for (;;) {
        curInput = getchar();
        switch (curInput) {
            case EOF : // if getchar fails it returns EOF
                if (feof(stdin) != 0) { // yes, getchar has funny error checking like this
                    CHECK_ERRNO(fprintf(stderr, "getchar() in "
                                        "void *control(void *) caused eof.\n"));
                } else if (ferror(stdin) != 0) {
                    CHECK_ERRNO(fprintf(stderr, "getchar() in "
                                        "void *control(void *) caused an error.\n"));
                }
                break;
            case '1' :
                CHECK(pthread_mutex_lock(&status.prod1Control.pauseMutex));
                status.prod1Control.isPaused =
                    not status.prod1Control.isPaused; // toggle the pause status for producer 1
                if (not status.prod1Control.isPaused) {
                    CHECK(pthread_cond_signal(&status.prod1Control.pauseCondVar)); // if producer 1 is not paused now, let it know about being unpaused
                }
                printStatusChange("prod1 %s\n",
                                  status.prod1Control.isPaused,
                                  pausedText, unPausedText); // let printStatusChange print the status change about producer 1
                CHECK(pthread_mutex_unlock(&status.prod1Control.pauseMutex));
                break;
            case '2' : // just like producer 1, really
                CHECK(pthread_mutex_lock(&status.prod2Control.pauseMutex));
                status.prod2Control.isPaused =
                    not status.prod2Control.isPaused;
                if (not status.prod2Control.isPaused) {
                    CHECK(pthread_cond_signal(&status.prod2Control.pauseCondVar));
                }
                printStatusChange("prod2 %s\n",
                                  status.prod2Control.isPaused,
                                  pausedText, unPausedText);
                CHECK(pthread_mutex_unlock(&status.prod2Control.pauseMutex));
                break;
            case 'c' : case 'C' : // the same thing, but for the consumer thread
                CHECK(pthread_mutex_lock(&status.consControl.pauseMutex));
                status.consControl.isPaused =
                    not status.consControl.isPaused;
                if (not status.consControl.isPaused) {
                    CHECK(pthread_cond_signal(&status.consControl.pauseCondVar));
                }
                printStatusChange("cons %s\n",
                                  status.consControl.isPaused,
                                  pausedText, unPausedText);
                CHECK(pthread_mutex_unlock(&status.consControl.pauseMutex));
                break;
            case 'q' : case 'Q' :
                CHECK_ERRNO(printf("shutting down...\n"));
                killConsAndProds(); // kill producer 1, producer 2 and consumer
                return (void *)EXIT_SUCCESS; // kill this thread (control)
            case 'h' : // print the help below
                CHECK_ERRNO(printf("1: Start / Stop Producer_1\n"
                                   "2: Start / Stop Producer_2\n"
                                   "c or C: Start / Stop Consumer\n"
                                   "q or Q: Kill all threads"
                                   " and close the application\n"
                                   "h: print this help.\n"));
                break;
            default : // ignore all other keyboard inputs
                break;
        }
    }
}

#define AMT_OF_CHARS_AFTER_WHICH_NEWLINE_MUST_BE_PRINTED    30

/*
all the thread have the same basic pattern:
- sleep
- lock mutex
- wait for the condvar (if we can't work on the buffer, because it's full (as a producer), or because it's empty (as a consumer)) indicating that we can work on the shared data (not_full for producers and not_empty for the consumer
- wait for the condvar (if the thread had been paused) indicating that the thread has been unpaused
- if we had to wait to be unpaused, go back up to check again if we can still mutate the buffer
- see if the thread is supposed to be alive, if not, kill it
- work on the shared data
- if the state of the buffer allows for other threads to access it now, let them know notifying via the associated cond_var
- unlock the mutex

-- note that running into a cond_wait implicitly unlocks the mutex, and waking up from that cond_wait by being notified implicitly tries to acquire (lock) the mutex (the thread will be blocked until it can acquire the mutex)
*/

void *p_1_w(void *pid) {
    CHECK_ERRNO(printf("Producer_1, PID: %d launched\n",
                       *(int *)pid));
    bool wasJustResumed;
    for (int i = 0; ; ++i) {
        sleep(PROD_SLEEP_TIME);
        CHECK(pthread_mutex_lock(&rb_mutex));
        do {
            wasJustResumed = false;
            while (isBufferFull()) {
                CHECK(pthread_cond_wait(&not_full_condvar,
                                        &rb_mutex));
            }

            CHECK(pthread_mutex_lock(&status.prod1Control.pauseMutex));
            while (status.prod1Control.isPaused) {
                CHECK(pthread_mutex_unlock(&rb_mutex));
                CHECK(pthread_cond_wait(&status.prod1Control.pauseCondVar,
                                        &status.prod1Control.pauseMutex));
                CHECK(pthread_mutex_lock(&rb_mutex));
                wasJustResumed = true;
            }
            CHECK(pthread_mutex_unlock(&status.prod1Control.pauseMutex));
        } while (wasJustResumed); // go back up to check if the resource is still available if we were just unpaused    
        CHECK(pthread_mutex_lock(&status.prod1Control.aliveMutex));
        if (not status.prod1Control.isAlive) {
            return exitThread(&status.prod1Control.aliveMutex);
        }
        CHECK(pthread_mutex_unlock(&status.prod1Control.aliveMutex));
        if (i >= CHARS_IN_ALPHABET) {
            i = 0;
        }
        CHECK_ERRNO(printf("\nProducer_1 wrote %c\n",
                           writeToBuffer('a', i)));

        if (bufferHasContent()) {
            CHECK(pthread_cond_broadcast(&not_empty_condvar));
        }
        CHECK(pthread_mutex_unlock(&rb_mutex));
    }
}

void *p_2_w(void *pid) {
    CHECK_ERRNO(printf("Producer_2, PID: %d launched\n",
                       *(int *)pid));
    bool wasJustResumed;
    for (int i = 0; ; ++i) {
        sleep(PROD_SLEEP_TIME);
        CHECK(pthread_mutex_lock(&rb_mutex));
        do {
            wasJustResumed = false;
            while (isBufferFull()) {
                CHECK(pthread_cond_wait(&not_full_condvar,
                                        &rb_mutex));
            }

            CHECK(pthread_mutex_lock(&status.prod2Control.pauseMutex));
            while (status.prod2Control.isPaused) {
                CHECK(pthread_mutex_unlock(&rb_mutex));

                CHECK(pthread_cond_wait(&status.prod2Control.pauseCondVar,
                                        &status.prod2Control.pauseMutex));
                CHECK(pthread_mutex_lock(&rb_mutex));
                wasJustResumed = true;
            }
            CHECK(pthread_mutex_unlock(&status.prod2Control.pauseMutex));
        } while (wasJustResumed); // go back up to check if the resource is still available if we were just unpaused  
        CHECK(pthread_mutex_lock(&status.prod2Control.aliveMutex));
        if (not status.prod2Control.isAlive) {
            return exitThread(&status.prod2Control.aliveMutex);
        }
        CHECK(pthread_mutex_unlock(&status.prod2Control.aliveMutex));
        if (i == CHARS_IN_ALPHABET) {
            i = 0;
        }
        CHECK_ERRNO(printf("\nProducer_2 wrote %c\n", writeToBuffer('A', i)));

        if (bufferHasContent()) {
            CHECK(pthread_cond_broadcast(&not_empty_condvar));
        }
        CHECK(pthread_mutex_unlock(&rb_mutex));
    }
}

void *consumer(void *pid) {
    CHECK_ERRNO(printf("Consumer, PID: %d launched\n",
                       *(int *)pid));
    bool wasJustResumed;
    size_t putNextCharAtThisIndex = 0U;
    for (int i = 1; ; ++i) {
        sleep(CONS_SLEEP_TIME);
        CHECK(pthread_mutex_lock(&rb_mutex));
        do {
            wasJustResumed = false;
            while (isBufferEmpty()) {
                CHECK(pthread_cond_wait(&not_empty_condvar,
                                        &rb_mutex));
            }

            CHECK(pthread_mutex_lock(&status.consControl.pauseMutex));
            while (status.consControl.isPaused) {
                CHECK(pthread_mutex_unlock(&rb_mutex));

                CHECK(pthread_cond_wait(&status.consControl.pauseCondVar,
                                        &status.consControl.pauseMutex));
                CHECK(pthread_mutex_lock(&rb_mutex));
                wasJustResumed = true;
            }
            CHECK(pthread_mutex_unlock(&status.consControl.pauseMutex));
        } while (wasJustResumed); // go back up to check if the resource is still available if we were just unpaused  
        CHECK(pthread_mutex_lock(&status.consControl.aliveMutex));
        if (not status.consControl.isAlive) {
            return exitThread(&status.consControl.aliveMutex);
        }
        CHECK(pthread_mutex_unlock(&status.consControl.aliveMutex));
        
        if (putNextCharAtThisIndex + 2U > consPrintBuffer.size) { 
            printBufferEnlarge(&consPrintBuffer, (putNextCharAtThisIndex + 2U) * 2U);
        }

        char charJustRead = readFromBuffer();
        consPrintBuffer.buf[putNextCharAtThisIndex] = charJustRead;
        ++putNextCharAtThisIndex;
        if (i >= AMT_OF_CHARS_AFTER_WHICH_NEWLINE_MUST_BE_PRINTED) {
            consPrintBuffer.buf[putNextCharAtThisIndex] = '\n';
            ++putNextCharAtThisIndex;
            i = 0; // 0 'cuz it goes back to 1 at the end of this loop
        }
        consPrintBuffer.buf[putNextCharAtThisIndex] = '\0'; // don't increment the putNextCharAtThisIndex variable here, 'cuz we wanna overwrite this one the next time

        CHECK_ERRNO(printf(consPrintBuffer.buf));
        
        if (bufferCanHoldMore()) {
            CHECK(pthread_cond_broadcast(&not_full_condvar));
        }

        CHECK_ERRNO(printf("\n"));

        CHECK(pthread_mutex_unlock(&rb_mutex));
    }
}
