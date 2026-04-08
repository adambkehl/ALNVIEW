/* Tests for GDB.c: genome database loading with malformed input */

#include "test_harness.h"
#include "GDB.h"
#include "ANO.h"
#include <sys/stat.h>

/* Helper: write a FASTA file */
static char *write_fasta(const char *content)
{
    return th_write_temp_file(".fasta", content, strlen(content));
}

/*  Get_GDB with various bad inputs  */

TEST(gdb_get_nonexistent)
{
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, "/tmp/no_such_file_xyz", NULL, 1, NULL);
    ASSERT_NULL(fps);
}

TEST(gdb_get_empty_fasta)
{
    char *path = write_fasta("");
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    /* Empty FASTA should fail or produce empty GDB */
    if (fps) {
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST(gdb_fasta_no_header)
{
    /* Sequence data but no >header line */
    char *path = write_fasta("ACGTACGTACGT\nGGGGAAAACCCC\n");
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    /* Should fail: no FASTA header */
    if (fps) {
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST(gdb_fasta_empty_sequence)
{
    /* Header followed by another header (empty sequence) */
    char *path = write_fasta(">scaffold_1\n>scaffold_2\nACGT\n");
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    if (fps) {
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST(gdb_fasta_valid_minimal)
{
    char *path = write_fasta(">scaf1\nACGTACGT\n");
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    if (fps) {
        ASSERT_TRUE(gdb.nscaff >= 1);
        ASSERT_TRUE(gdb.ncontig >= 1);
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST(gdb_fasta_with_gaps)
{
    /* Scaffold with gap (N's) between contigs */
    char *path = write_fasta(">scaf1\nACGTACGT\nNNNNNNNNNN\nGGCCGGCC\n");
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    if (fps) {
        ASSERT_TRUE(gdb.nscaff >= 1);
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST(gdb_fasta_long_header)
{
    /* Header much longer than initial allocation */
    char header[8192];
    header[0] = '>';
    memset(header + 1, 'X', 8190);
    header[8191] = '\0';

    char content[8300];
    snprintf(content, sizeof(content), "%s\nACGTACGT\n", header);
    char *path = write_fasta(content);

    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    if (fps) {
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST(gdb_fasta_many_scaffolds)
{
    /* Many scaffolds to trigger reallocation of scaffold/contig arrays */
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    for (int i = 0; i < 200; i++)
        fprintf(f, ">scaffold_%d\nACGTACGT\n", i);
    fflush(f);

    /* Write to a real path */
    rewind(f);
    char *path = th_write_temp_file(".fasta", "", 0);
    FILE *out = fopen(path, "w");
    char buf[256];
    while (fgets(buf, sizeof(buf), f))
        fputs(buf, out);
    fclose(out);
    fclose(f);

    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    if (fps) {
        ASSERT_EQ(gdb.nscaff, 200);
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST(gdb_fasta_mixed_case_masking)
{
    /* Lowercase indicates soft-masking */
    char *path = write_fasta(">scaf1\nACGTacgtACGT\n");
    GDB gdb;
    ANO ano;
    memset(&gdb, 0, sizeof(GDB));
    memset(&ano, 0, sizeof(ANO));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, &ano);
    if (fps) {
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        if (ano.nints > 0) {
            /* ANO shares our GDB, mark as shared to prevent double-close */
            ano.shared = 1;
            Free_ANO(&ano);
        }
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST(gdb_fasta_only_ns)
{
    /* Scaffold with only N's (no contigs, just gaps) */
    char *path = write_fasta(">scaf1\nNNNNNNNNNNNNNNNNNNNN\n");
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    if (fps) {
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST(gdb_fasta_single_base)
{
    char *path = write_fasta(">s\nA\n");
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    if (fps) {
        ASSERT_TRUE(gdb.ncontig >= 1);
        ASSERT_EQ(gdb.contigs[0].clen, 1);
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

/*  Read_GDB with malformed .1gdb  */

TEST(read_gdb_nonexistent)
{
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    int rc = Read_GDB(&gdb, "/tmp/no_such.1gdb");
    ASSERT_NE(rc, 0);
}

/*  Load_Sequences edge cases  */

TEST(load_sequences_no_bps)
{
    /* Create GDB from FASTA with num_bps=0 (no .bps file) then try Load_Sequences */
    char *path = write_fasta(">s\nACGTACGT\n");
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    if (fps) {
        /* Load into memory as NUMERIC */
        int rc = Load_Sequences(&gdb, NUMERIC);
        if (rc == 0) {
            ASSERT_TRUE(gdb.seqstate == NUMERIC);
        }
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

/*  New_Contig_Buffer  */

TEST(contig_buffer_valid_gdb)
{
    char *path = write_fasta(">s\nACGTACGTACGT\n");
    GDB gdb;
    memset(&gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(&gdb, path, NULL, 1, NULL);
    if (fps) {
        char *buf = New_Contig_Buffer(&gdb);
        ASSERT_NOT_NULL(buf);
        free(buf - 1);  /* -1 for prefix sentinel */
        /* fps points into gdb struct when num_bps=1, do NOT free it */
        Close_GDB(&gdb);
    }
    unlink(path);
}

TEST_MAIN()
