/* Tests for select.c: region selection parsing with malformed input */

#include "test_harness.h"
#include "gene_core.h"
#include "GDB.h"
#include "hash.h"
#include "ANO.h"
#include "select.h"

/* Helper: set up GDB + hash */
static int setup(GDB *gdb, Hash_Table **hash, char **fasta_path)
{
    const char *fasta =
        ">scaffold_1\nACGTACGTACGTACGTACGTACGTACGTACGT\n"
        ">scaffold_2\nGGCCGGCCGGCCGGCC\n"
        ">scaffold_3\nAAAACCCCGGGGTTTT\n";
    *fasta_path = th_write_temp_file(".fasta", fasta, strlen(fasta));
    memset(gdb, 0, sizeof(GDB));
    FILE **fps = Get_GDB(gdb, *fasta_path, NULL, 1, NULL);
    if (!fps) return 1;
    /* fps points into gdb struct when num_bps=1, do NOT free it */

    *hash = New_Hash_Table(64, 1);
    if (!*hash) { Close_GDB(gdb); return 1; }
    for (int i = 0; i < gdb->nscaff; i++)
        Hash_Add(*hash, gdb->headers + gdb->scaffolds[i].hoff);
    return 0;
}

static void teardown(GDB *gdb, Hash_Table *hash, char *fasta_path)
{
    Free_Hash_Table(hash);
    Close_GDB(gdb);
    unlink(fasta_path);
}

/*  get_selection_contigs  */

TEST(select_empty_expression)
{
    GDB gdb;
    Hash_Table *hash;
    char *fpath;
    if (setup(&gdb, &hash, &fpath)) return;

    Contig_Range *cr = get_selection_contigs("", &gdb, hash, 0);
    /* Empty expression — should return NULL or all-zero ranges */
    if (cr) free(cr);

    teardown(&gdb, hash, fpath);
}

TEST(select_valid_scaffold_name)
{
    GDB gdb;
    Hash_Table *hash;
    char *fpath;
    if (setup(&gdb, &hash, &fpath)) return;

    Contig_Range *cr = get_selection_contigs("scaffold_1", &gdb, hash, 0);
    if (cr) {
        /* First contig (scaffold_1's) should be selected */
        ASSERT_TRUE(cr[0].order > 0);
        free(cr);
    }

    teardown(&gdb, hash, fpath);
}

TEST(select_nonexistent_scaffold)
{
    GDB gdb;
    Hash_Table *hash;
    char *fpath;
    if (setup(&gdb, &hash, &fpath)) return;

    Contig_Range *cr = get_selection_contigs("NONEXISTENT_SCAFFOLD", &gdb, hash, 0);
    /* Should handle gracefully — error or empty result */
    if (cr) free(cr);

    teardown(&gdb, hash, fpath);
}

/*  interpret_range  */

TEST(interpret_range_empty)
{
    GDB gdb;
    Hash_Table *hash;
    char *fpath;
    if (setup(&gdb, &hash, &fpath)) return;

    Selection sel;
    memset(&sel, 0, sizeof(Selection));
    int rc = interpret_range(&sel, "", &gdb, hash);
    /* Empty should fail or produce empty result */
    (void)rc;

    teardown(&gdb, hash, fpath);
}

TEST(interpret_range_valid)
{
    GDB gdb;
    Hash_Table *hash;
    char *fpath;
    if (setup(&gdb, &hash, &fpath)) return;

    Selection sel;
    memset(&sel, 0, sizeof(Selection));
    int rc = interpret_range(&sel, "scaffold_1", &gdb, hash);
    (void)rc;

    teardown(&gdb, hash, fpath);
}

/*  interpret_point  */

TEST(interpret_point_empty)
{
    GDB gdb;
    Hash_Table *hash;
    char *fpath;
    if (setup(&gdb, &hash, &fpath)) return;

    Selection sel;
    memset(&sel, 0, sizeof(Selection));
    int rc = interpret_point(&sel, "", &gdb, hash, &gdb, hash);
    (void)rc;

    teardown(&gdb, hash, fpath);
}

TEST(interpret_point_garbage)
{
    GDB gdb;
    Hash_Table *hash;
    char *fpath;
    if (setup(&gdb, &hash, &fpath)) return;

    Selection sel;
    memset(&sel, 0, sizeof(Selection));
    int rc = interpret_point(&sel, "!!!@@@###$$$", &gdb, hash, &gdb, hash);
    (void)rc;

    teardown(&gdb, hash, fpath);
}

TEST_MAIN()
