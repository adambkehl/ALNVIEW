/* Tests for alncode.c: .1aln file parsing with malformed input */

#include "test_harness.h"
#include "alncode.h"

static char *aln_schema_text =
    "1 3 def 2 1\n"
    ".\n"
    "P 3 seq\n"
    "O s 2 3 INT 6 STRING\n"
    "G S\n"
    "D n 2 4 CHAR 3 INT\n"
    "O S 1 3 DNA\n"
    "D I 1 6 STRING\n"
    ".\n"
    "P 3 aln\n"
    "D t 1 3 INT\n"
    ".\n"
    "O g 0\n"
    "G S\n"
    "O S 1 6 STRING\n"
    "D G 1 3 INT\n"
    "D C 1 3 INT\n"
    ".\n"
    "O a 0\n"
    "G A\n"
    "D p 2 3 INT 3 INT\n"
    ".\n"
    "O A 6 3 INT 3 INT 3 INT 3 INT 3 INT 3 INT\n"
    "D L 2 3 INT 3 INT\n"
    "D R 0\n"
    "D D 1 3 INT\n"
    "D T 1 8 INT_LIST\n"
    "D X 1 8 INT_LIST\n"
    "D Q 1 3 INT\n"
    "D E 1 3 INT\n"
    "D Z 1 6 STRING\n"
    "D U 1 3 INT\n";

/* Helper: create a minimal valid .1aln ASCII file */
static char *write_minimal_aln(void)
{
    const char *content =
        "1 3 aln\n"
        "< 6 db1.fa 1\n"
        "< 6 db2.fa 2\n"
        "t 100\n"
        "A 0 0 1000 1 0 1000\n"
        "D 10\n"
        "T 2 5 5\n"
        "X 2 3 3\n";
    return th_write_temp_file(".1aln", content, strlen(content));
}

/*  Opening  */

TEST(aln_open_nonexistent)
{
    int64 novl;
    int tspace;
    char *db1, *db2, *cpath;
    OneFile *of = open_Aln_Read("/tmp/no_such_file.1aln", 1,
                                &novl, &tspace, &db1, &db2, &cpath);
    ASSERT_NULL(of);
}

TEST(aln_open_empty_file)
{
    char *path = th_write_temp_file(".1aln", "", 0);
    int64 novl;
    int tspace;
    char *db1, *db2, *cpath;
    OneFile *of = open_Aln_Read(path, 1, &novl, &tspace, &db1, &db2, &cpath);
    ASSERT_NULL(of);
    unlink(path);
}

TEST(aln_open_garbage)
{
    char garbage[128];
    memset(garbage, 0xFF, sizeof(garbage));
    char *path = th_write_temp_file(".1aln", garbage, sizeof(garbage));
    int64 novl;
    int tspace;
    char *db1, *db2, *cpath;
    OneFile *of = open_Aln_Read(path, 1, &novl, &tspace, &db1, &db2, &cpath);
    ASSERT_NULL(of);
    unlink(path);
}

TEST(aln_open_no_references)
{
    /* Valid aln header but no reference lines */
    const char *content =
        "1 3 aln\n"
        "t 100\n"
        "A 0 0 1000 1 0 1000\n"
        "T 2 5 5\n"
        "X 2 3 3\n";
    char *path = th_write_temp_file(".1aln", content, strlen(content));
    int64 novl;
    int tspace;
    char *db1, *db2, *cpath;
    OneFile *of = open_Aln_Read(path, 1, &novl, &tspace, &db1, &db2, &cpath);
    /* Should fail: no references */
    ASSERT_NULL(of);
    unlink(path);
}

TEST(aln_open_no_tline)
{
    /* Has references but no t-line (trace spacing) */
    const char *content =
        "1 3 aln\n"
        "< 6 db1.fa 1\n"
        "A 0 0 1000 1 0 1000\n"
        "T 2 5 5\n"
        "X 2 3 3\n";
    char *path = th_write_temp_file(".1aln", content, strlen(content));
    int64 novl;
    int tspace;
    char *db1, *db2, *cpath;
    OneFile *of = open_Aln_Read(path, 1, &novl, &tspace, &db1, &db2, &cpath);
    /* Should fail: t-line with value 0 or missing */
    if (of) oneFileClose(of);
    else ASSERT_NULL(of);
    unlink(path);
}

TEST(aln_open_valid_minimal)
{
    char *path = write_minimal_aln();
    int64 novl;
    int tspace;
    char *db1 = NULL, *db2 = NULL, *cpath = NULL;
    OneFile *of = open_Aln_Read(path, 1, &novl, &tspace, &db1, &db2, &cpath);
    if (of) {
        ASSERT_EQ(tspace, 100);
        oneFileClose(of);
    }
    free(db1);
    free(db2);
    if (cpath && cpath[0] != '\0') free(cpath);
    unlink(path);
}

/*  make_Aln_Schema  */

TEST(aln_schema_create)
{
    OneSchema *s = make_Aln_Schema();
    ASSERT_NOT_NULL(s);
    oneSchemaDestroy(s);
}

TEST_MAIN()
