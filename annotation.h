#ifndef _ANNOTATION
#define _ANNOTATION

#include "gene_core.h"
#include "GDB.h"
#include "hash.h"

/*******************************************************************************************
 *
 *  CHROMOSOME ALIAS MAPPING
 *
 ********************************************************************************************/

typedef struct
  { const char *alias;       //  e.g. "chr1"
    const char *accession;   //  e.g. "NC_060925.1"
  } ChromAlias;

  //  Add chr1-22,X,Y,M aliases for CHM13 v2.0 accessions into the hash table.
  //  Returns the number of aliases successfully matched and added, or -1 on error.

int Add_Chrom_Aliases(Hash_Table *hash, GDB *gdb);

  //  Given a scaffold index, return the chromosome alias (e.g. "chr1") if one
  //  exists for that scaffold's accession name. Returns NULL if no alias found.

const char *Chrom_Alias(GDB *gdb, int scaf_idx);


/*******************************************************************************************
 *
 *  ANNOTATION TRACK DATA STRUCTURES
 *
 ********************************************************************************************/

typedef enum
  { FEAT_GENE = 0,
    FEAT_EXON,
    FEAT_CDS,
    FEAT_UTR5,
    FEAT_UTR3,
    FEAT_MRNA,
    FEAT_CENTROMERE,
    FEAT_SATELLITE,
    FEAT_CYTOBAND,
    FEAT_TELOMERE,
    FEAT_SEGDUP,
    FEAT_REPEAT,
    FEAT_GENOME_FEATURE,
    FEAT_OTHER,
    NUM_FEAT_TYPES
  } FeatureType;

typedef struct
  { int64       beg;       //  absolute genome coordinate (0-based)
    int64       end;       //  absolute genome coordinate (exclusive)
    FeatureType type;
    int8        strand;    //  0 = +, 1 = -, 2 = .
    int         score;
    int         label;     //  offset into AnnotTrack labels block, or -1
  } GenomeFeature;

typedef struct
  { int            nfeat;      //  total number of features
    GenomeFeature *features;   //  sorted by beg coordinate
    int64         *soff;       //  soff[s]..soff[s+1] = features for scaffold s
    int            nscaff;     //  number of scaffolds
    char          *labels;     //  label string pool
    int            label_size; //  bytes used in labels
    int            label_max;  //  bytes allocated for labels
  } AnnotTrack;


/*******************************************************************************************
 *
 *  BED AND GFF3 PARSERS
 *
 ********************************************************************************************/

  //  Read a BED file into an AnnotTrack. All features get the given default_type
  //  unless the file contains a feature type column. Coordinates are converted
  //  to absolute genome positions using gdb and hash. Returns 0 on success.

int Read_BED(AnnotTrack *track, char *path, GDB *gdb, Hash_Table *hash,
             FeatureType default_type);

  //  Read a GFF3 file (optionally gzipped) into an AnnotTrack. Only gene, exon,
  //  CDS, mRNA, five_prime_UTR, three_prime_UTR features are loaded. Returns 0
  //  on success.

int Read_GFF3(AnnotTrack *track, char *path, GDB *gdb, Hash_Table *hash);


/*******************************************************************************************
 *
 *  TRACK QUERIES
 *
 ********************************************************************************************/

  //  Find all features in [beg, end) range. Sets *out to the first matching
  //  feature in the sorted array and returns the count. No allocation is
  //  performed -- *out points into the track's features array.

int Query_Track(AnnotTrack *track, GDB *gdb, int64 beg, int64 end,
                GenomeFeature **out);

  //  Free all memory in an AnnotTrack (but not the struct itself).

void Free_AnnotTrack(AnnotTrack *track);

#endif
