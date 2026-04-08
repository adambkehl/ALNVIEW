/* Tests for ANO.c: annotation file parsing with malformed input */

#include "test_harness.h"
#include "ANO.h"
#include "GDB.h"

/* Helper: create a GDB from a simple FASTA */
static int make_test_gdb(GDB *gdb, char **path_out)
{
    const char *fasta = ">scaffold_1\nACGTACGTACGTACGTACGT\n>scaffold_2\nGGCCGGCCGGCC\n";
    *path_out = th_write_temp_file(".fasta", fasta, strlen(fasta));
    memset(gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(gdb, *path_out, NULL, 1, NULL);
    if (!fps) return 1;
    fclose(fps[0]);
    free(fps);
    return 0;
}

/*  Read_ANO with bad paths  */

TEST(ano_read_nonexistent)
{
    ANO ano;
    memset(&ano, 0, sizeof(ANO));
    int rc = Read_ANO(&ano, "/tmp/no_such_file.1ano", NULL);
    ASSERT_NE(rc, 0);
}

/*  make_ANO_Schema  */

TEST(ano_schema_create)
{
    OneSchema *s = make_ANO_Schema();
    ASSERT_NOT_NULL(s);
    oneSchemaDestroy(s);
}

/*  Free_ANO on zeroed structure  */

TEST(ano_free_empty)
{
    ANO ano;
    memset(&ano, 0, sizeof(ANO));
    /* Freeing a zeroed ANO should not crash (free(NULL) is safe) */
    Free_ANO(&ano);
}

/*  Show_ANO on empty  */

TEST(ano_show_empty)
{
    ANO ano;
    memset(&ano, 0, sizeof(ANO));
    /* Should not crash even with no data */
    /* Redirect stdout to suppress output */
    FILE *devnull = fopen("/dev/null", "w");
    if (devnull) {
        int saved = dup(1);
        dup2(fileno(devnull), 1);
        Show_ANO(&ano);
        fflush(stdout);
        dup2(saved, 1);
        close(saved);
        fclose(devnull);
    }
}

TEST_MAIN()
