#ifndef TEST_HARNESS_HPP
#define TEST_HARNESS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "papi.h"  // for PAPI_* error codes used by helper macros

struct HarnessOpts {
    bool print;
    bool expect_fail;
    int  had_warning;   // set to 1 if we hit ENOEVNT/ECNFLCT or any warning
};

static HarnessOpts harness_opts;

// Accept the positional token "TESTS_QUIET" (or "QUIET"):
//  - remove it from argv so it can't be mistaken for a positional arg,
//  - enable quiet mode via env for the harness to pick up.
static inline void harness_accept_tests_quiet(int *argc, char **argv) {
    /* The PAPI test harness historically invokes each test with a single
       positional token holding the value of the TESTS_QUIET environment
       variable.  Only the literal string "TESTS_QUIET" should trigger quiet
       mode.  If any other value is present we drop it from argv and ignore
       the environment variable so tests don't misinterpret it as a positional
       argument. */

    char *badarg = NULL;
    const char *tq_env = getenv("TESTS_QUIET");
    if (tq_env && strcmp(tq_env, "TESTS_QUIET") != 0) {
        badarg = strdup(tq_env);  // remember stray value to filter from argv
        unsetenv("TESTS_QUIET");  // ignore non‑literal TESTS_QUIET
    }

    int w = 1;
    int saw_quiet = 0;
    for (int r = 1; r < *argc; ++r) {
        const char *a = argv[r];
        if (a && (!strcmp(a, "TESTS_QUIET") || !strcmp(a, "QUIET"))) {
            saw_quiet = 1;
            continue;
        }
        if (badarg && a && strcmp(a, badarg) == 0) {
            /* discard unexpected TESTS_QUIET value */
            continue;
        }
        argv[w++] = argv[r];
    }
    argv[w] = NULL;
    *argc = w;
    if (saw_quiet) setenv("TESTS_QUIET", "1", 1);
    if (badarg) free(badarg);
}

// Parse CLI
static HarnessOpts parse_harness_cli(int argc, char **argv) {
    harness_opts.print = false;
    harness_opts.expect_fail = false;
    harness_opts.had_warning = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--print") == 0) harness_opts.print = true;
        else if (strncmp(argv[i], "--expect=", 9) == 0) {
            const char *v = argv[i] + 9;
            harness_opts.expect_fail = (strcmp(v, "fail") == 0);
        }
    }
    if (!harness_opts.print) {
        const char *tq = getenv("TESTS_QUIET");
        if (tq && *tq) harness_opts.print = false;
    }
    if (!harness_opts.print) {
        const char* q = getenv("PAPI_AMDSMI_TEST_QUIET");
        if (!q || q[0] != '1') setenv("PAPI_AMDSMI_TEST_QUIET", "1", 1);
    } else {
        unsetenv("PAPI_AMDSMI_TEST_QUIET");
    }
    return harness_opts;
}

// Final status line
static int eval_result(const HarnessOpts &opts, int result_code) {
    bool passed = opts.expect_fail ? (result_code != 0) : (result_code == 0);
    if (passed) {
        if (opts.had_warning) printf("PASSED with WARNING\n");
        else                  printf("PASSED\n");
    } else {
        printf("FAILED!!!\n");  // always the shouty failure you wanted
    }
    return passed ? 0 : 1;
}

// Print note only with --print
#define NOTE(...) do { \
    if (harness_opts.print) { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } \
} while (0)

// Mark a warning (does not exit)
#define WARNF(...) do { \
    harness_opts.had_warning = 1; \
    if (harness_opts.print) { fprintf(stdout, "WARNING: "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } \
} while (0)

// === Cant conduct helpers ===
// Exit immediately as PASSED with WARNING (portable success)
#define EXIT_WARNING(...) do { \
    harness_opts.had_warning = 1; \
    if (harness_opts.print && *#__VA_ARGS__) { fprintf(stdout, "WARNING: "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } \
    printf("PASSED with WARNING\n"); fflush(stdout); exit(0); \
} while (0)

// If add fails due to unsupported or HW/resource limits end as PASSED with WARNING
#define EXIT_WARNING_ON_ADD(rc, evname) do { \
    if ((rc) == PAPI_ENOEVNT || (rc) == PAPI_ECNFLCT || (rc) == PAPI_EPERM) { \
        EXIT_WARNING("Event unavailable (%s): %s", \
            ((rc) == PAPI_ENOEVNT ? "ENOEVNT" : (rc) == PAPI_ECNFLCT ? "ECNFLCT" : "EPERM"), (evname)); \
    } \
} while (0)

// If start fails due to HW/resource limits end as PASSED with WARNING
#define EXIT_WARNING_ON_START(rc, ctx) do { \
    if ((rc) == PAPI_ECNFLCT || (rc) == PAPI_EPERM) { \
        EXIT_WARNING("Cannot start counters (%s): %s", (ctx), PAPI_strerror(rc)); \
    } \
} while (0)

// Keep SKIP as a cant conduct success-with-warning
#define SKIP(reason) EXIT_WARNING("%s", (reason))

#endif // TEST_HARNESS_HPP
