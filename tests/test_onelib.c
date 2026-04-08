/* Tests for ONElib.c: ONE format parsing with malformed input */

#include "test_harness.h"
#include "ONElib.h"
#include "gene_core.h"

/* Helper: write a text ONE file to a temp path and return the path */
static char *write_one_file(const char *content)
{
    return th_write_temp_file(".1test", content, strlen(content));
}

/*  Schema creation  */

TEST(schema_valid)
{
    OneSchema *s = oneSchemaCreateFromText(
        "P 3 seq\n"
        "O S 1 3 DNA\n"
    );
    ASSERT_NOT_NULL(s);
    oneSchemaDestroy(s);
}

XFAIL_TEST(schema_empty_string)
{
    /* BUG: oneSchemaCreateFromText calls die() on invalid schemas instead of
       returning NULL.  die() calls exit() which bypasses interactive mode. */
    OneSchema *s = oneSchemaCreateFromText("");
    /* Empty schema should fail or return NULL */
    if (s) oneSchemaDestroy(s);
}

XFAIL_TEST(schema_garbage)
{
    /* BUG: Same as above — die() instead of returning NULL */
    OneSchema *s = oneSchemaCreateFromText("this is not a schema\nrandom garbage\n");
    if (s) oneSchemaDestroy(s);
}

TEST(schema_null)
{
    OneSchema *s = oneSchemaCreateFromText(NULL);
    /* Should not crash */
    if (s) oneSchemaDestroy(s);
}

/*  File opening  */

TEST(open_nonexistent_file)
{
    OneSchema *s = oneSchemaCreateFromText("P 3 seq\nO S 1 3 DNA\n");
    if (!s) return;
    OneFile *of = oneFileOpenRead("/tmp/nonexistent_file_12345.1seq", s, "seq", 1);
    ASSERT_NULL(of);
    oneSchemaDestroy(s);
}

TEST(open_empty_file)
{
    char *path = th_write_temp_file(".1seq", "", 0);
    OneSchema *s = oneSchemaCreateFromText("P 3 seq\nO S 1 3 DNA\n");
    if (!s) return;
    OneFile *of = oneFileOpenRead(path, s, "seq", 1);
    /* Empty file should fail to open or have no records */
    if (of) oneFileClose(of);
    oneSchemaDestroy(s);
    unlink(path);
}

TEST(open_truncated_binary_header)
{
    /* Write just a few bytes that look like a ONE file start but are truncated */
    char data[] = "1 3 seq\n";
    char *path = th_write_temp_file(".1seq", data, strlen(data));
    OneSchema *s = oneSchemaCreateFromText("P 3 seq\nO S 1 3 DNA\n");
    if (!s) { unlink(path); return; }
    OneFile *of = oneFileOpenRead(path, s, "seq", 1);
    if (of) {
        /* Try reading — should get nothing or fail gracefully */
        oneReadLine(of);
        oneFileClose(of);
    }
    oneSchemaDestroy(s);
    unlink(path);
}

TEST(open_wrong_type)
{
    /* Create a valid seq file but open as aln — type mismatch */
    char *path = th_write_temp_file(".1seq", "1 3 seq\n", 8);
    OneSchema *s = oneSchemaCreateFromText("P 3 aln\nD t 1 3 INT\n");
    if (!s) { unlink(path); return; }
    OneFile *of = oneFileOpenRead(path, s, "aln", 1);
    /* Should fail due to type mismatch */
    if (of) oneFileClose(of);
    oneSchemaDestroy(s);
    unlink(path);
}

/*  Reading malformed content  */

TEST(read_file_with_only_header)
{
    /* Valid header but no data lines */
    const char *content = "1 3 seq\n! 4 test 3 1.0 7 testing 27 Mon Apr 07 12:00:00 2025\n";
    char *path = write_one_file(content);
    OneSchema *s = oneSchemaCreateFromText("P 3 seq\nO S 1 3 DNA\n");
    if (!s) { unlink(path); return; }
    OneFile *of = oneFileOpenRead(path, s, "seq", 1);
    if (of) {
        /* Should return 0 (end of data) on first read */
        char lt = oneReadLine(of);
        ASSERT_EQ(lt, 0);
        oneFileClose(of);
    }
    oneSchemaDestroy(s);
    unlink(path);
}

TEST(read_file_random_bytes)
{
    /* Fill with random garbage */
    char garbage[256];
    for (int i = 0; i < 256; i++)
        garbage[i] = (char)(i ^ 0xA5);
    char *path = th_write_temp_file(".1seq", garbage, 256);
    OneSchema *s = oneSchemaCreateFromText("P 3 seq\nO S 1 3 DNA\n");
    if (!s) { unlink(path); return; }
    OneFile *of = oneFileOpenRead(path, s, "seq", 1);
    /* Should fail to open */
    if (of) oneFileClose(of);
    oneSchemaDestroy(s);
    unlink(path);
}

TEST(schema_with_many_types)
{
    /* Stress test: schema with many line types */
    const char *big_schema =
        "P 3 tst\n"
        "D A 1 3 INT\n"
        "D B 2 3 INT 4 REAL\n"
        "D C 1 6 STRING\n"
        "D D 1 8 INT_LIST\n"
        "D E 1 3 DNA\n"
        "D F 3 3 INT 3 INT 3 INT\n"
        "D G 0\n"
        "D H 1 4 CHAR\n";
    OneSchema *s = oneSchemaCreateFromText(big_schema);
    ASSERT_NOT_NULL(s);
    oneSchemaDestroy(s);
}

TEST_MAIN()
