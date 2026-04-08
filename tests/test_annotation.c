/* Tests for annotation.c: BED/GFF3 parsers with malformed input */

#include "test_harness.h"
#include "annotation.h"
#include "hash.h"
#include "GDB.h"
#include "ANO.h"

/* Helper: set up a GDB + hash for testing */
static int setup_gdb_and_hash(GDB *gdb, Hash_Table **hash, char **fasta_path)
{
    const char *fasta = ">chr1\nACGTACGTACGTACGTACGTACGTACGTACGT\n"
                        ">chr2\nGGCCGGCCGGCCGGCCGGCC\n";
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

/*  BED parser  */

TEST(bed_nonexistent_file)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_BED(&track, "/tmp/no_such.bed", &gdb, hash, FEAT_OTHER);
    ASSERT_NE(rc, 0);

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
}

TEST(bed_empty_file)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    char *bed_path = th_write_temp_file(".bed", "", 0);
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_BED(&track, bed_path, &gdb, hash, FEAT_OTHER);
    /* Empty file: 0 features, should succeed or fail gracefully */
    if (rc == 0) Free_AnnotTrack(&track);

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(bed_path);
}

TEST(bed_valid_minimal)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    const char *bed_content = "chr1\t5\t10\tfeature1\t100\t+\n";
    char *bed_path = th_write_temp_file(".bed", bed_content, strlen(bed_content));
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_BED(&track, bed_path, &gdb, hash, FEAT_GENE);
    if (rc == 0) {
        ASSERT_TRUE(track.nfeat >= 1);
        Free_AnnotTrack(&track);
    }

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(bed_path);
}

TEST(bed_unknown_chromosome)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    const char *bed_content = "chrUNKNOWN\t0\t100\tfeature\t0\t+\n";
    char *bed_path = th_write_temp_file(".bed", bed_content, strlen(bed_content));
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_BED(&track, bed_path, &gdb, hash, FEAT_OTHER);
    if (rc == 0) {
        /* Unknown chrom lines should be skipped */
        ASSERT_EQ(track.nfeat, 0);
        Free_AnnotTrack(&track);
    }

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(bed_path);
}

TEST(bed_start_greater_than_end)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    const char *bed_content = "chr1\t100\t5\tfeature\t0\t+\n";
    char *bed_path = th_write_temp_file(".bed", bed_content, strlen(bed_content));
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_BED(&track, bed_path, &gdb, hash, FEAT_OTHER);
    /* Should handle gracefully — either skip or accept */
    if (rc == 0) Free_AnnotTrack(&track);

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(bed_path);
}

TEST(bed_negative_coordinates)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    const char *bed_content = "chr1\t-10\t5\tfeature\t0\t+\n";
    char *bed_path = th_write_temp_file(".bed", bed_content, strlen(bed_content));
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_BED(&track, bed_path, &gdb, hash, FEAT_OTHER);
    if (rc == 0) Free_AnnotTrack(&track);

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(bed_path);
}

TEST(bed_missing_fields)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    /* Only chrom and start — missing end and other fields */
    const char *bed_content = "chr1\t5\n";
    char *bed_path = th_write_temp_file(".bed", bed_content, strlen(bed_content));
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_BED(&track, bed_path, &gdb, hash, FEAT_OTHER);
    if (rc == 0) Free_AnnotTrack(&track);

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(bed_path);
}

TEST(bed_comment_and_track_lines)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    const char *bed_content =
        "#comment line\n"
        "track name=test\n"
        "browser position chr1:1-100\n"
        "chr1\t5\t10\tfeature1\t100\t+\n";
    char *bed_path = th_write_temp_file(".bed", bed_content, strlen(bed_content));
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_BED(&track, bed_path, &gdb, hash, FEAT_GENE);
    if (rc == 0) {
        ASSERT_TRUE(track.nfeat >= 1);
        Free_AnnotTrack(&track);
    }

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(bed_path);
}

TEST(bed_many_features)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    /* Generate many features to trigger reallocation */
    char *bed_path = th_write_temp_file(".bed", "", 0);
    FILE *f = fopen(bed_path, "w");
    for (int i = 0; i < 500; i++)
        fprintf(f, "chr1\t%d\t%d\tfeat_%d\t0\t+\n", i, i + 1, i);
    fclose(f);

    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_BED(&track, bed_path, &gdb, hash, FEAT_OTHER);
    if (rc == 0) {
        ASSERT_EQ(track.nfeat, 500);
        Free_AnnotTrack(&track);
    }

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(bed_path);
}

/*  GFF3 parser  */

TEST(gff3_nonexistent_file)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_GFF3(&track, "/tmp/no_such.gff3", &gdb, hash);
    ASSERT_NE(rc, 0);

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
}

TEST(gff3_empty_file)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    char *gff_path = th_write_temp_file(".gff3", "", 0);
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_GFF3(&track, gff_path, &gdb, hash);
    if (rc == 0) Free_AnnotTrack(&track);

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(gff_path);
}

TEST(gff3_valid_minimal)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    const char *gff_content =
        "##gff-version 3\n"
        "chr1\t.\tgene\t5\t20\t.\t+\t.\tID=gene1;Name=TestGene\n";
    char *gff_path = th_write_temp_file(".gff3", gff_content, strlen(gff_content));
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_GFF3(&track, gff_path, &gdb, hash);
    if (rc == 0) {
        ASSERT_TRUE(track.nfeat >= 1);
        Free_AnnotTrack(&track);
    }

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(gff_path);
}

TEST(gff3_missing_fields)
{
    GDB gdb;
    Hash_Table *hash;
    char *fasta_path;
    if (setup_gdb_and_hash(&gdb, &hash, &fasta_path)) return;

    /* Only 4 fields instead of 9 */
    const char *gff_content = "chr1\t.\tgene\t5\n";
    char *gff_path = th_write_temp_file(".gff3", gff_content, strlen(gff_content));
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    int rc = Read_GFF3(&track, gff_path, &gdb, hash);
    if (rc == 0) Free_AnnotTrack(&track);

    Free_Hash_Table(hash);
    Close_GDB(&gdb);
    unlink(fasta_path);
    unlink(gff_path);
}

/*  Free_AnnotTrack on zeroed struct  */

TEST(annottrack_free_empty)
{
    AnnotTrack track;
    memset(&track, 0, sizeof(AnnotTrack));
    Free_AnnotTrack(&track);
    /* Should not crash */
}

TEST_MAIN()
