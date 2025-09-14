// amdsmi_ctx_conflict.cpp ? contention test (refactor)
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <pthread.h>
#include <unistd.h>
#include "papi.h"
#include "test_harness.hpp"

static unsigned long get_tid() { return (unsigned long)pthread_self(); }

struct ThreadState { int start_rc = PAPI_OK; };
static std::atomic<bool> t1_started(false);

// Default event; you can override with argv[1] (any amd_smi native event string)
static const char* g_event = "amd_smi:::temp_current:device=0:sensor=1";

static void* thread_fn1(void* arg) {
    PAPI_register_thread();
    ThreadState* st = reinterpret_cast<ThreadState*>(arg);

    int EventSet = PAPI_NULL;
    int rc = PAPI_create_eventset(&EventSet);
    if (rc != PAPI_OK) { NOTE("t1 create: %s", PAPI_strerror(rc)); st->start_rc = rc; PAPI_unregister_thread(); return nullptr; }

    rc = PAPI_add_named_event(EventSet, g_event);
    if (rc == PAPI_ENOEVNT) { SKIP("Event not supported on this platform"); }
    if (rc == PAPI_ECNFLCT || rc == PAPI_EPERM) { SKIP("Cannot add event due to HW/resource limits"); }
    if (rc != PAPI_OK) { NOTE("t1 add: %s", PAPI_strerror(rc)); st->start_rc = rc; PAPI_destroy_eventset(&EventSet); PAPI_unregister_thread(); return nullptr; }

    rc = PAPI_start(EventSet);
    st->start_rc = rc;
    if (rc == PAPI_OK) {
        t1_started.store(true, std::memory_order_release);
        long long v=0; (void)PAPI_read(EventSet, &v);
        usleep(100000); // keep running long enough for thread2 to collide
        (void)PAPI_stop(EventSet, &v);
    } else {
        // If thread 1 cannot start, we cannot conduct the test => PASS with WARNING
        SKIP("Cannot start thread1 due to HW/resource limits");
    }

    (void)PAPI_cleanup_eventset(EventSet);
    (void)PAPI_destroy_eventset(&EventSet);
    PAPI_unregister_thread();
    return nullptr;
}

static void* thread_fn2(void* arg) {
    PAPI_register_thread();
    ThreadState* st = reinterpret_cast<ThreadState*>(arg);

    int EventSet = PAPI_NULL;
    int rc = PAPI_create_eventset(&EventSet);
    if (rc != PAPI_OK) { NOTE("t2 create: %s", PAPI_strerror(rc)); st->start_rc = rc; PAPI_unregister_thread(); return nullptr; }

    rc = PAPI_add_named_event(EventSet, g_event);
    if (rc == PAPI_ENOEVNT) { SKIP("Event not supported on this platform"); }
    if (rc == PAPI_ECNFLCT || rc == PAPI_EPERM) { SKIP("Cannot add event due to HW/resource limits"); }
    if (rc != PAPI_OK) { NOTE("t2 add: %s", PAPI_strerror(rc)); st->start_rc = rc; PAPI_destroy_eventset(&EventSet); PAPI_unregister_thread(); return nullptr; }

    // Wait until thread1 is actually running the event
    while (!t1_started.load(std::memory_order_acquire)) { /* spin */ }

    rc = PAPI_start(EventSet);
    st->start_rc = rc;
    if (rc != PAPI_OK) {
        NOTE("t2 start expected fail: %s", PAPI_strerror(rc));
    } else {
        NOTE("t2 start unexpectedly succeeded");
        long long v=0; (void)PAPI_stop(EventSet, &v);
    }

    (void)PAPI_cleanup_eventset(EventSet);
    (void)PAPI_destroy_eventset(&EventSet);
    PAPI_unregister_thread();
    return nullptr;
}

int main(int argc, char** argv) {
    // Unbuffer stdout so the final status line always shows promptly
    setvbuf(stdout, nullptr, _IONBF, 0);

    harness_accept_tests_quiet(&argc, argv);
    auto opts = parse_harness_cli(argc, argv);
    // Optional override of the event: ./amdsmi_ctx_conflict "<event>"
    if (argc > 1 && strncmp(argv[1], "--", 2) != 0) g_event = argv[1];

    const char* root = std::getenv("PAPI_AMDSMI_ROOT");
    if (!root || !*root) SKIP("PAPI_AMDSMI_ROOT not set");

    int rc = PAPI_library_init(PAPI_VER_CURRENT);
    if (rc != PAPI_VER_CURRENT) { NOTE("PAPI_library_init failed: %s", PAPI_strerror(rc)); int e = eval_result(opts, 1); fflush(stdout); return e; }

    if (PAPI_thread_init(&get_tid) != PAPI_OK) { NOTE("PAPI_thread_init failed"); int e = eval_result(opts, 1); fflush(stdout); return e; }

    t1_started.store(false, std::memory_order_relaxed);

    ThreadState s1{}, s2{};
    pthread_t th1, th2;
    pthread_create(&th1, nullptr, thread_fn1, &s1);
    pthread_create(&th2, nullptr, thread_fn2, &s2);
    pthread_join(th1, nullptr);
    pthread_join(th2, nullptr);

    if (opts.print) {
        printf("event: %s\n", g_event);
        printf("t1 start rc: %d (%s)\n", s1.start_rc, PAPI_strerror(s1.start_rc));
        printf("t2 start rc: %d (%s)\n", s2.start_rc, PAPI_strerror(s2.start_rc));
    }

    // PASS when expected contention occurred; else FAIL.
    int final_rc = (s1.start_rc == PAPI_OK && s2.start_rc == PAPI_ECNFLCT) ? 0 : 1;
    if (final_rc != 0) NOTE("Unexpected results (wanted t1 OK, t2 PAPI_ECNFLCT).");

    int exit_code = eval_result(opts, final_rc);
    fflush(stdout);
    return exit_code;
}
