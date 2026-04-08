/* Tests for gene_core.c: memory helpers, string utilities, error handling */

#include "test_harness.h"
#include "gene_core.h"

/*  Malloc  */

TEST(malloc_normal)
{
    void *p = Malloc(100, "test alloc");
    ASSERT_NOT_NULL(p);
    memset(p, 0xAA, 100);  /* Should not trigger ASan */
    free(p);
}

TEST(malloc_zero_size)
{
    /* malloc(0) is implementation-defined but shouldn't crash */
    void *p = Malloc(0, "zero alloc");
    /* p may be NULL or valid — just don't crash */
    free(p);
}

XFAIL_TEST(malloc_negative_size)
{
    /* BUG: Malloc passes negative int64 directly to malloc() where it wraps
       to a huge size_t.  Malloc should check size <= 0 (like Realloc does).
       ASan correctly aborts on the oversized allocation. */
    void *p = Malloc(-1, "negative alloc");
    ASSERT_NULL(p);
}

/*  Realloc  */

TEST(realloc_null_ptr)
{
    /* realloc(NULL, size) is equivalent to malloc(size) */
    void *p = Realloc(NULL, 64, "realloc null");
    ASSERT_NOT_NULL(p);
    free(p);
}

TEST(realloc_zero_size)
{
    /* Realloc explicitly clamps size <= 0 to 1 */
    void *p = Malloc(64, "initial");
    ASSERT_NOT_NULL(p);
    void *q = Realloc(p, 0, "realloc zero");
    ASSERT_NOT_NULL(q);
    free(q);
}

TEST(realloc_negative_size)
{
    /* Realloc clamps size <= 0 to 1, so -1 becomes 1 */
    void *p = Malloc(64, "initial");
    ASSERT_NOT_NULL(p);
    void *q = Realloc(p, -1, "realloc neg");
    ASSERT_NOT_NULL(q);
    free(q);
}

/*  Strdup  */

TEST(strdup_normal)
{
    char *s = Strdup("hello", "test strdup");
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ(s, "hello");
    free(s);
}

TEST(strdup_null)
{
    /* Strdup(NULL) returns NULL without error */
    char *s = Strdup(NULL, "null strdup");
    ASSERT_NULL(s);
}

TEST(strdup_empty)
{
    char *s = Strdup("", "empty strdup");
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ(s, "");
    free(s);
}

/*  Strndup  */

TEST(strndup_normal)
{
    char *s = Strndup("hello world", 5, "test strndup");
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ(s, "hello");
    free(s);
}

TEST(strndup_null)
{
    char *s = Strndup(NULL, 5, "null strndup");
    ASSERT_NULL(s);
}

/*  PathTo  */

TEST(pathto_with_slash)
{
    char *p = PathTo("/foo/bar/baz.txt");
    ASSERT_NOT_NULL(p);
    ASSERT_STR_EQ(p, "/foo/bar");
    free(p);
}

TEST(pathto_no_slash)
{
    char *p = PathTo("filename.txt");
    ASSERT_NOT_NULL(p);
    ASSERT_STR_EQ(p, ".");
    free(p);
}

TEST(pathto_null)
{
    char *p = PathTo(NULL);
    ASSERT_NULL(p);
}

/*  Root  */

TEST(root_with_suffix)
{
    char *r = Root("/path/to/file.1aln", ".1aln");
    ASSERT_NOT_NULL(r);
    ASSERT_STR_EQ(r, "file");
    free(r);
}

TEST(root_no_suffix_match)
{
    char *r = Root("/path/to/file.txt", ".1aln");
    ASSERT_NOT_NULL(r);
    ASSERT_STR_EQ(r, "file.txt");
    free(r);
}

TEST(root_null)
{
    char *r = Root(NULL, ".1aln");
    ASSERT_NULL(r);
}

/*  Catenate  */

TEST(catenate_normal)
{
    char *c = Catenate("/path", "/", "file", ".1aln");
    ASSERT_NOT_NULL(c);
    ASSERT_STR_EQ(c, "/path/file.1aln");
}

TEST(catenate_null_arg)
{
    char *c = Catenate(NULL, "/", "file", ".1aln");
    ASSERT_NULL(c);
}

TEST(catenate_empty_strings)
{
    char *c = Catenate("", "", "", "");
    ASSERT_NOT_NULL(c);
    ASSERT_STR_EQ(c, "");
}

/*  Numbered_Suffix  */

TEST(numbered_suffix_normal)
{
    char *s = Numbered_Suffix("._gdb.", 12345, ".bps");
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ(s, "._gdb.12345.bps");
}

TEST(numbered_suffix_null)
{
    char *s = Numbered_Suffix(NULL, 0, ".bps");
    ASSERT_NULL(s);
}

/*  Number_Digits  */

TEST(number_digits_zero)
{
    ASSERT_EQ(Number_Digits(0), 1);
}

TEST(number_digits_positive)
{
    ASSERT_EQ(Number_Digits(999), 3);
    ASSERT_EQ(Number_Digits(1000), 4);
}

TEST(number_digits_negative)
{
    ASSERT_EQ(Number_Digits(-42), 3);  /* minus sign + 2 digits */
}

/*  Compress/Uncompress  */

TEST(compress_uncompress_roundtrip)
{
    /* Allocate with sentinels like the real code does */
    char *buf = (char *)malloc(100 + 2);
    ASSERT_NOT_NULL(buf);
    buf += 1;  /* prefix sentinel */

    /* Fill with numeric DNA: 0,1,2,3 pattern */
    int len = 20;
    for (int i = 0; i < len; i++)
        buf[i] = (char)(i % 4);
    buf[len] = 4;  /* sentinel */

    char orig[20];
    memcpy(orig, buf, 20);

    Compress_Read(len, buf);
    Uncompress_Read(len, buf, 0);

    for (int i = 0; i < len; i++)
        ASSERT_EQ(buf[i], orig[i]);

    free(buf - 1);
}

/*  EPRINTF  */

TEST(eprintf_fills_buffer)
{
    th_error_buf[0] = '\0';
    EPRINTF("test error %d", 42);
    ASSERT_TRUE(strstr(th_error_buf, "test error 42") != NULL);
}

TEST(eprintf_long_message)
{
    /* Messages longer than ERROR_BUFFER_LEN should be truncated, not overflow */
    char longmsg[16384];
    memset(longmsg, 'A', sizeof(longmsg) - 1);
    longmsg[sizeof(longmsg) - 1] = '\0';
    EPRINTF("%s", longmsg);
    /* If we get here without ASan complaint, truncation worked */
    ASSERT_TRUE(strlen(th_error_buf) < ERROR_BUFFER_LEN);
}

TEST_MAIN()
