#include "Header.h"

pthread_mutex_t rb_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty_condvar = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full_condvar = PTHREAD_COND_INITIALIZER;

#define THREAD_CONTROL_INIT_LIST   PTHREAD_MUTEX_INITIALIZER, \
                                   PTHREAD_COND_INITIALIZER, \
                                   false, PTHREAD_MUTEX_INITIALIZER, \
                                   true
Status status = {
    { THREAD_CONTROL_INIT_LIST }, // initialise all the member structs(type ThreadControl) of status
    { THREAD_CONTROL_INIT_LIST }, // see Header.h for the types ThreadControl and Status
    { THREAD_CONTROL_INIT_LIST }
};
#undef THREAD_CONTROL_INIT_LIST

int thread_id[NUM_THREADS] = { 0, 1, 2, 3 };

RingBuffer x = { { 0 }, NULL, NULL, 0 };
RingBuffer * const p_rb = &x;

PrintBuffer consPrintBuffer;

#ifdef _WIN32
#   define SCHED_POLICY SCHED_OTHER    // Pthreads-w32 only supports SCHED_OTHER
#else
#   define SCHED_POLICY SCHED_FIFO
#endif

#define LOOP_NUM_THREADS    for (i = 0; i < NUM_THREADS; ++i) 

int main(void) {
    setbuf(stdout, NULL); // use unbuffered output, so that we can immediately see the output without having to flush
    printBufferConstruct(&consPrintBuffer, PRINT_BUFFER_BASE_SIZE);
    int i; // Counter for the for loops used in main
    pthread_t threads[NUM_THREADS];
    pthread_attr_t threadAttributes[NUM_THREADS];
    int policies[NUM_THREADS] = {
        SCHED_POLICY, SCHED_POLICY,
        SCHED_POLICY, SCHED_POLICY };
    void *threadReturnVals[NUM_THREADS]; // this is where we will store the return values of the threads
    struct sched_param schedParams[NUM_THREADS];
    int priorities[NUM_THREADS];
    struct sched_param dummyParam; // dummy scheduling parameter to be used for checking purposes, see the code below
    int dummyInheritSched; // dummy values
    int dummyPolicy; // used for checking
    void *(*threadFunctions[NUM_THREADS])(void *) // array of pointers to functions, taking void *, returning void *
        = { p_1_w, p_2_w, consumer, control }; // this holds pointers to the thread functions
    CHECK_ERRNO(printf("Start des Beispiels\n"));
    LOOP_NUM_THREADS { // initialize priorities
        priorities[i] = sched_get_priority_min(SCHED_POLICY);
    }

    LOOP_NUM_THREADS {
        CHECK(pthread_attr_init(&threadAttributes[i])); // initialise the attributes, the corresponding destroy function calls are to be found at the end of main

        CHECK(pthread_attr_setinheritsched(&threadAttributes[i], // let us explicitly set the scheduling policy, otherwise the ones specified in our thread attributes will be ignored and those of the parent thread (main) will be used
                                           PTHREAD_EXPLICIT_SCHED));
        CHECK(pthread_attr_getinheritsched(&threadAttributes[i], // get it back so we can check it
                                           &dummyInheritSched));
        assert(PTHREAD_EXPLICIT_SCHED == dummyInheritSched // assert that it was actually set
               and "PTHREAD_EXPLICIT_SCHED didn't get set.");

        CHECK(pthread_attr_setschedpolicy(&threadAttributes[i], // now let's actually set the scheduling policy
                                          policies[i]));
        CHECK(pthread_attr_getschedpolicy(&threadAttributes[i], // and now get it back so we can see if it worked
                                          &dummyPolicy));
        assert(policies[i] == dummyPolicy and
               "pthread_attr_getschedpolicy didn't"
               " return the scheduling policy that was set");

        schedParams[i].sched_priority = priorities[i]; // set the priorities
        CHECK(pthread_attr_setschedparam(&threadAttributes[i], // give the sched_params to the pthread_attr_ts
                                         &schedParams[i]));
        CHECK(pthread_attr_getschedparam(&threadAttributes[i], // get the sched_params copied into the dummy so we can check them.
                                         &dummyParam));
        assert(schedParams[i].sched_priority ==
                dummyParam.sched_priority and
               "pthread_attr_getschedparam didn't return the same"
               " sched_params set by pthread_attr_setschedparam"); // ensure that we get back the same priority that we put in
    }

    assert((p_rb->count == 0
            and p_rb->p_in == NULL
            and p_rb->p_out == NULL)
            and "the ringbuffer was in an unexpected state."); // make sure the ringbuffer is the way it's supposed to be, this should never fail, really

    p_rb->p_in = P_START; // set the in and out pointers the beginning of the buffer
    p_rb->p_out = P_START;
    p_rb->count = 0; // at first there are no characters to be read
    CHECK_ERRNO(printf("Counter value %d\n", p_rb->count));

    LOOP_NUM_THREADS {
        CHECK(pthread_create(&threads[i], &threadAttributes[i],
                             threadFunctions[i],
                             (void *)&thread_id[i])); // create all the threads with their thread attributes
    }

    LOOP_NUM_THREADS {
        CHECK(pthread_join(threads[i], // wait for the threads to return
                           &threadReturnVals[i]));
    }

    LOOP_NUM_THREADS {
        assert((threadReturnVals[i] == (void *)EXIT_SUCCESS) and
               "one or more threads did not exit successfully"); // assert that all threads returned EXIT_SUCCESS (0)
    }

    LOOP_NUM_THREADS {
        CHECK(pthread_attr_destroy(&threadAttributes[i])); // don't forget to clean up these thread attributes once we're done.
    }

    CHECK_ERRNO(printf("Ende nach Join der Threads\n"));

    printBufferDestroy(&consPrintBuffer);

    sleep(3U); // wait a little before exiting.

    return EXIT_SUCCESS;
}
#undef LOOP_NUM_THREADS
