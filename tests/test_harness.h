/*  Minimal C test harness with fork-based isolation and ASan compatibility.
 *  Each test runs in a child process so crashes don't kill the suite.
 */

#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <setjmp.h>

/* ---- Counters ---- */
static int th_pass = 0;
static int th_fail = 0;
static int th_crash = 0;
static int th_xfail = 0;
static int th_total = 0;

/* ---- Error buffer for interactive-mode C code ---- */
static char th_error_buf[8192];

/* ---- Test registration ---- */
typedef void (*th_test_fn)(void);

typedef struct {
    const char *label;
    th_test_fn  fn;
    int         xfail;  /* 1 = expected failure (crash or ASan error) */
} th_test_entry;

#define MAX_TESTS 512
static th_test_entry th_tests[MAX_TESTS];
static int th_test_count = 0;

/* Helper to register a test without macro-parameter/field-name collision */
static inline void th_register(const char *label, th_test_fn fn, int xfail)
{
    th_tests[th_test_count].label = label;
    th_tests[th_test_count].fn    = fn;
    th_tests[th_test_count].xfail = xfail;
    th_test_count++;
}

#define TEST(name)                                                  \
    static void test_##name(void);                                  \
    __attribute__((constructor)) static void reg_##name(void) {     \
        th_register(#name, test_##name, 0);                         \
    }                                                               \
    static void test_##name(void)

#define XFAIL_TEST(name)                                            \
    static void test_##name(void);                                  \
    __attribute__((constructor)) static void reg_##name(void) {     \
        th_register(#name, test_##name, 1);                         \
    }                                                               \
    static void test_##name(void)

/* ---- Assertions ---- */
#define ASSERT_TRUE(expr) do {                                      \
    if (!(expr)) {                                                  \
        fprintf(stderr, "  ASSERT_TRUE failed: %s (%s:%d)\n",      \
                #expr, __FILE__, __LINE__);                         \
        _exit(1);                                                   \
    }                                                               \
} while(0)

#define ASSERT_FALSE(expr) do {                                     \
    if ((expr)) {                                                   \
        fprintf(stderr, "  ASSERT_FALSE failed: %s (%s:%d)\n",     \
                #expr, __FILE__, __LINE__);                         \
        _exit(1);                                                   \
    }                                                               \
} while(0)

#define ASSERT_NULL(expr) do {                                      \
    if ((expr) != NULL) {                                           \
        fprintf(stderr, "  ASSERT_NULL failed: %s (%s:%d)\n",      \
                #expr, __FILE__, __LINE__);                         \
        _exit(1);                                                   \
    }                                                               \
} while(0)

#define ASSERT_NOT_NULL(expr) do {                                  \
    if ((expr) == NULL) {                                           \
        fprintf(stderr, "  ASSERT_NOT_NULL failed: %s (%s:%d)\n",  \
                #expr, __FILE__, __LINE__);                         \
        _exit(1);                                                   \
    }                                                               \
} while(0)

#define ASSERT_EQ(a, b) do {                                        \
    if ((a) != (b)) {                                               \
        fprintf(stderr, "  ASSERT_EQ failed: %s != %s (%s:%d)\n",  \
                #a, #b, __FILE__, __LINE__);                        \
        _exit(1);                                                   \
    }                                                               \
} while(0)

#define ASSERT_NE(a, b) do {                                        \
    if ((a) == (b)) {                                               \
        fprintf(stderr, "  ASSERT_NE failed: %s == %s (%s:%d)\n",  \
                #a, #b, __FILE__, __LINE__);                        \
        _exit(1);                                                   \
    }                                                               \
} while(0)

#define ASSERT_STR_EQ(a, b) do {                                    \
    if (strcmp((a), (b)) != 0) {                                    \
        fprintf(stderr, "  ASSERT_STR_EQ failed: \"%s\" != \"%s\"" \
                " (%s:%d)\n", (a), (b), __FILE__, __LINE__);       \
        _exit(1);                                                   \
    }                                                               \
} while(0)

/* ---- Setup: call before tests to enable interactive-mode error handling ---- */
static void th_setup_error_buffer(void)
{
    extern char *Error_Buffer;
    extern char *Prog_Name;
    Error_Buffer = th_error_buf;
    Prog_Name = strdup("test");
}

/* ---- Temp file helpers ---- */
static char *th_write_temp_file(const char *suffix, const void *data, size_t len)
{
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/th_test_XXXXXX%s", suffix ? suffix : "");
    int fd = mkstemps(path, suffix ? (int)strlen(suffix) : 0);
    if (fd < 0) { perror("mkstemps"); return NULL; }
    if (len > 0) write(fd, data, len);
    close(fd);
    return path;
}

/* ---- Runner ---- */
static int th_run_all(void)
{
    printf("Running %d tests...\n\n", th_test_count);

    for (int i = 0; i < th_test_count; i++) {
        th_total++;
        fflush(stdout);
        fflush(stderr);

        pid_t pid = fork();
        if (pid == 0) {
            /* Child: run the test */
            th_setup_error_buffer();
            th_tests[i].fn();
            _exit(0);  /* success */
        }

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            if (th_tests[i].xfail) {
                printf("  XPASS %s (expected failure but passed)\n", th_tests[i].label);
                th_pass++;
            } else {
                printf("  PASS  %s\n", th_tests[i].label);
                th_pass++;
            }
        } else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (th_tests[i].xfail) {
                printf("  XFAIL %s (signal %d - expected)\n", th_tests[i].label, sig);
                th_xfail++;
            } else {
                printf("  CRASH %s (signal %d: %s)\n", th_tests[i].label, sig,
                       sig == SIGSEGV ? "SIGSEGV" :
                       sig == SIGABRT ? "SIGABRT" :
                       sig == SIGBUS  ? "SIGBUS"  : "other");
                th_crash++;
            }
        } else {
            int code = WEXITSTATUS(status);
            if (th_tests[i].xfail) {
                printf("  XFAIL %s (exit %d - expected)\n", th_tests[i].label, code);
                th_xfail++;
            } else {
                printf("  FAIL  %s (exit %d)\n", th_tests[i].label, code);
                th_fail++;
            }
        }
    }

    printf("\n--- Results ---\n");
    printf("  Total: %d  Pass: %d  Fail: %d  Crash: %d  XFail: %d\n",
           th_total, th_pass, th_fail, th_crash, th_xfail);

    return (th_fail + th_crash > 0) ? 1 : 0;
}

/* Default main — just run all registered tests */
#define TEST_MAIN()                         \
    int main(int argc, char *argv[]) {      \
        (void)argc; (void)argv;             \
        return th_run_all();                \
    }

#endif /* TEST_HARNESS_H */
