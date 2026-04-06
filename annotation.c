/*******************************************************************************************
 *
 *  Annotation track support for ALNview: chromosome aliases, BED/GFF3 parsing,
 *  and efficient interval queries.
 *
 *  Author:  Adam Kehl
 *
 ********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zlib.h>

#include "annotation.h"


/*******************************************************************************************
 *
 *  CHM13 v2.0 CHROMOSOME ALIAS TABLE
 *
 ********************************************************************************************/

static ChromAlias chm13v2_aliases[] =
  { { "chr1",  "NC_060925.1" },
    { "chr2",  "NC_060926.1" },
    { "chr3",  "NC_060927.1" },
    { "chr4",  "NC_060928.1" },
    { "chr5",  "NC_060929.1" },
    { "chr6",  "NC_060930.1" },
    { "chr7",  "NC_060931.1" },
    { "chr8",  "NC_060932.1" },
    { "chr9",  "NC_060933.1" },
    { "chr10", "NC_060934.1" },
    { "chr11", "NC_060935.1" },
    { "chr12", "NC_060936.1" },
    { "chr13", "NC_060937.1" },
    { "chr14", "NC_060938.1" },
    { "chr15", "NC_060939.1" },
    { "chr16", "NC_060940.1" },
    { "chr17", "NC_060941.1" },
    { "chr18", "NC_060942.1" },
    { "chr19", "NC_060943.1" },
    { "chr20", "NC_060944.1" },
    { "chr21", "NC_060945.1" },
    { "chr22", "NC_060946.1" },
    { "chrX",  "NC_060947.1" },
    { "chrY",  "NC_060948.1" },
    { "chrM",  "NC_012920.1" },
  };

#define NUM_CHM13_ALIASES  (int)(sizeof(chm13v2_aliases) / sizeof(ChromAlias))

int Add_Chrom_Aliases(Hash_Table *hash, GDB *gdb)
{ int i, s, matched;
  GDB_SCAFFOLD *scf;
  char         *headers;

  scf     = gdb->scaffolds;
  headers = gdb->headers;
  matched = 0;

  //  For each alias, find the accession in the hash table. If found,
  //  overwrite the scaffold header string with the alias (alias is always
  //  shorter than the accession, so this is safe).

  for (i = 0; i < NUM_CHM13_ALIASES; i++)
    { int idx = Hash_Lookup(hash, (char *) chm13v2_aliases[i].accession);
      if (idx >= 0)
        { strcpy(headers + scf[idx].hoff, chm13v2_aliases[i].alias);
          matched += 1;
        }
    }

  if (matched == 0)
    return (0);

  //  Rebuild the hash table so lookups by alias name return the
  //  correct scaffold index (same index as the original accession).

  Clear_Hash_Table(hash);
  for (s = 0; s < gdb->nscaff; s++)
    { if (Hash_Add(hash, headers + scf[s].hoff) < 0)
        return (-1);
    }

  return (matched);
}

const char *Chrom_Alias(GDB *gdb, int scaf_idx)
{ char *name;
  int   i;

  if (scaf_idx < 0 || scaf_idx >= gdb->nscaff)
    return (NULL);

  //  After Add_Chrom_Aliases, the header string is already the alias
  //  (e.g. "chr1"). Check if it matches any known alias.

  name = gdb->headers + gdb->scaffolds[scaf_idx].hoff;

  for (i = 0; i < NUM_CHM13_ALIASES; i++)
    { if (strcmp(name, chm13v2_aliases[i].alias) == 0)
        return (chm13v2_aliases[i].alias);
    }

  return (NULL);
}


/*******************************************************************************************
 *
 *  INTERNAL HELPERS
 *
 ********************************************************************************************/

  //  Find scaffold index for a chromosome name using the hash table

static int find_scaffold(char *chrom, Hash_Table *hash)
{ int idx;

  idx = Hash_Lookup(hash, chrom);
  if (idx >= 0)
    return (idx);

  return (-1);
}

  //  Convert scaffold-relative position to absolute genome coordinate

static int64 scaf_to_abs(GDB *gdb, int scaf_idx, int64 pos)
{ return (gdb->contigs[gdb->scaffolds[scaf_idx].fctg].sbeg + pos);
}

  //  Comparison function for sorting GenomeFeature by beg coordinate

static int feat_cmp(const void *a, const void *b)
{ const GenomeFeature *fa = (const GenomeFeature *) a;
  const GenomeFeature *fb = (const GenomeFeature *) b;

  if (fa->beg < fb->beg) return (-1);
  if (fa->beg > fb->beg) return (1);
  if (fa->end < fb->end) return (-1);
  if (fa->end > fb->end) return (1);
  return (0);
}

  //  Build the per-scaffold offset index (soff) after features are sorted

static int build_soff(AnnotTrack *track, GDB *gdb)
{ int64 *soff;
  int    i, s, nscaff;

  nscaff = gdb->nscaff;
  track->nscaff = nscaff;

  soff = (int64 *) Malloc(sizeof(int64) * (nscaff + 1), "Allocating soff");
  if (soff == NULL)
    return (1);

  //  Find scaffold boundaries using scaffold start positions

  for (s = 0; s <= nscaff; s++)
    soff[s] = track->nfeat;

  s = 0;
  soff[0] = 0;
  for (i = 0; i < track->nfeat; i++)
    { int64 fbeg = track->features[i].beg;
      while (s < nscaff - 1)
        { int64 next_scaf_start = gdb->contigs[gdb->scaffolds[s+1].fctg].sbeg;
          if (fbeg < next_scaf_start)
            break;
          s += 1;
          soff[s] = i;
        }
    }
  for (s = s + 1; s <= nscaff; s++)
    soff[s] = track->nfeat;

  track->soff = soff;
  return (0);
}

  //  Ensure the label pool has room for len more bytes

static int ensure_label_space(AnnotTrack *track, int len)
{ if (track->label_size + len + 1 > track->label_max)
    { int new_max = track->label_max * 2;
      char *new_labels;
      if (new_max < track->label_size + len + 1)
        new_max = track->label_size + len + 1;
      new_labels = (char *) Realloc(track->labels, new_max, "Expanding labels");
      if (new_labels == NULL)
        return (1);
      track->labels = new_labels;
      track->label_max = new_max;
    }
  return (0);
}

  //  Add a label string to the pool, return offset into pool (-1 on error)

static int add_label(AnnotTrack *track, const char *str)
{ int len = strlen(str);
  int off;

  if (ensure_label_space(track, len))
    return (-1);

  off = track->label_size;
  memcpy(track->labels + off, str, len + 1);
  track->label_size += len + 1;
  return (off);
}


/*******************************************************************************************
 *
 *  BED PARSER
 *
 ********************************************************************************************/

#define INIT_FEAT_CAP  10000
#define LINE_BUF_SIZE  65536

int Read_BED(AnnotTrack *track, char *path, GDB *gdb, Hash_Table *hash,
             FeatureType default_type)
{ gzFile       fp;
  char         line[LINE_BUF_SIZE];
  int          cap, n;
  GenomeFeature *feats;

  fp = gzopen(path, "r");
  if (fp == NULL)
    { EPRINTF("Cannot open BED file: %s\n", path);
      return (1);
    }

  cap  = INIT_FEAT_CAP;
  n    = 0;
  feats = (GenomeFeature *) Malloc(sizeof(GenomeFeature) * cap, "Allocating features");
  if (feats == NULL)
    { gzclose(fp);
      return (1);
    }

  track->labels    = (char *) Malloc(4096, "Allocating labels");
  track->label_max = 4096;
  track->label_size = 0;
  if (track->labels == NULL)
    { free(feats);
      gzclose(fp);
      return (1);
    }

  while (gzgets(fp, line, LINE_BUF_SIZE) != NULL)
    { char *chrom, *p;
      int64 start, end;
      int   scaf_idx;
      char  name_buf[1024];
      int   score;
      char  strand_ch;

      //  Skip comment and header lines
      if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
        continue;
      if (strncmp(line, "browser", 7) == 0 || strncmp(line, "track", 5) == 0)
        continue;

      //  Parse chrom
      chrom = line;
      p = chrom;
      while (*p != '\t' && *p != '\0')
        p++;
      if (*p == '\0') continue;
      *p++ = '\0';

      //  Parse start
      start = strtoll(p, &p, 10);
      if (*p == '\t') p++;
      else continue;

      //  Parse end
      end = strtoll(p, &p, 10);

      //  Lookup scaffold
      scaf_idx = find_scaffold(chrom, hash);
      if (scaf_idx < 0)
        continue;   //  Unknown chromosome, skip

      //  Optional: name (column 4)
      name_buf[0] = '\0';
      if (*p == '\t')
        { char *nb;
          p++;
          nb = name_buf;
          while (*p != '\t' && *p != '\n' && *p != '\r' && *p != '\0' && nb - name_buf < 1023)
            *nb++ = *p++;
          *nb = '\0';
        }

      //  Optional: score (column 5)
      score = 0;
      if (*p == '\t')
        { p++;
          score = strtol(p, &p, 10);
        }

      //  Optional: strand (column 6)
      strand_ch = '.';
      if (*p == '\t')
        { p++;
          if (*p == '+' || *p == '-' || *p == '.')
            strand_ch = *p;
        }

      //  Grow array if needed
      if (n >= cap)
        { cap = cap * 2;
          feats = (GenomeFeature *) Realloc(feats, sizeof(GenomeFeature) * cap,
                                            "Expanding features");
          if (feats == NULL)
            { gzclose(fp);
              return (1);
            }
        }

      feats[n].beg    = scaf_to_abs(gdb, scaf_idx, start);
      feats[n].end    = scaf_to_abs(gdb, scaf_idx, end);
      feats[n].type   = default_type;
      feats[n].strand = (strand_ch == '+') ? 0 : (strand_ch == '-') ? 1 : 2;
      feats[n].score  = score;

      if (name_buf[0] != '\0' && strcmp(name_buf, ".") != 0)
        feats[n].label = add_label(track, name_buf);
      else
        feats[n].label = -1;

      n += 1;
    }

  gzclose(fp);

  track->nfeat    = n;
  track->features = feats;

  //  Sort by position
  qsort(feats, n, sizeof(GenomeFeature), feat_cmp);

  //  Build per-scaffold index
  if (build_soff(track, gdb))
    return (1);

  return (0);
}


/*******************************************************************************************
 *
 *  GFF3 PARSER
 *
 ********************************************************************************************/

static FeatureType gff3_type(const char *type_str)
{ if (strcmp(type_str, "gene") == 0)            return (FEAT_GENE);
  if (strcmp(type_str, "exon") == 0)            return (FEAT_EXON);
  if (strcmp(type_str, "CDS") == 0)             return (FEAT_CDS);
  if (strcmp(type_str, "five_prime_UTR") == 0)  return (FEAT_UTR5);
  if (strcmp(type_str, "three_prime_UTR") == 0) return (FEAT_UTR3);
  if (strcmp(type_str, "mRNA") == 0)            return (FEAT_MRNA);
  if (strcmp(type_str, "pseudogene") == 0)      return (FEAT_GENE);
  if (strcmp(type_str, "lnc_RNA") == 0)         return (FEAT_GENE);
  if (strcmp(type_str, "rRNA") == 0)            return (FEAT_GENE);
  if (strcmp(type_str, "tRNA") == 0)            return (FEAT_GENE);
  if (strcmp(type_str, "transcript") == 0)      return (FEAT_MRNA);
  return (NUM_FEAT_TYPES);   //  sentinel: skip this feature
}

  //  Extract a value from GFF3 attributes for a given key (e.g. "Name=", "gene=")
  //  Returns pointer into attrs or NULL. Copies value into buf.

static char *extract_attr(const char *attrs, const char *key, char *buf, int buflen)
{ const char *p;
  int klen = strlen(key);

  p = attrs;
  while (*p != '\0')
    { if (strncmp(p, key, klen) == 0)
        { const char *v = p + klen;
          int i = 0;
          while (*v != ';' && *v != '\n' && *v != '\r' && *v != '\0' && i < buflen - 1)
            buf[i++] = *v++;
          buf[i] = '\0';
          return (buf);
        }
      //  Advance to next attribute (after ';')
      while (*p != ';' && *p != '\0')
        p++;
      if (*p == ';')
        p++;
    }
  return (NULL);
}

int Read_GFF3(AnnotTrack *track, char *path, GDB *gdb, Hash_Table *hash)
{ gzFile       fp;
  char         line[LINE_BUF_SIZE];
  int          cap, n;
  GenomeFeature *feats;

  fp = gzopen(path, "r");
  if (fp == NULL)
    { EPRINTF("Cannot open GFF3 file: %s\n", path);
      return (1);
    }

  cap  = INIT_FEAT_CAP;
  n    = 0;
  feats = (GenomeFeature *) Malloc(sizeof(GenomeFeature) * cap, "Allocating features");
  if (feats == NULL)
    { gzclose(fp);
      return (1);
    }

  track->labels     = (char *) Malloc(65536, "Allocating labels");
  track->label_max  = 65536;
  track->label_size = 0;
  if (track->labels == NULL)
    { free(feats);
      gzclose(fp);
      return (1);
    }

  while (gzgets(fp, line, LINE_BUF_SIZE) != NULL)
    { char *fields[9];
      char *p, *tok;
      int   f, scaf_idx;
      int64 start, end;
      FeatureType ftype;
      char  name_buf[1024];
      char  strand_ch;
      int   score;

      if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
        continue;

      //  Split into 9 tab-delimited fields
      tok = line;
      for (f = 0; f < 9; f++)
        { fields[f] = tok;
          p = tok;
          while (*p != '\t' && *p != '\n' && *p != '\r' && *p != '\0')
            p++;
          if (*p == '\0' || *p == '\n' || *p == '\r')
            { *p = '\0';
              f++;
              break;
            }
          *p = '\0';
          tok = p + 1;
        }
      if (f < 9)
        continue;   //  incomplete line

      //  fields[0]=seqid  [1]=source  [2]=type  [3]=start  [4]=end
      //  fields[5]=score  [6]=strand  [7]=phase [8]=attributes

      //  Check feature type
      ftype = gff3_type(fields[2]);
      if (ftype == NUM_FEAT_TYPES)
        continue;   //  skip unrecognized types

      //  Lookup scaffold
      scaf_idx = find_scaffold(fields[0], hash);
      if (scaf_idx < 0)
        continue;

      //  GFF3 is 1-based inclusive, convert to 0-based half-open
      start = strtoll(fields[3], NULL, 10) - 1;
      end   = strtoll(fields[4], NULL, 10);

      score = 0;
      if (fields[5][0] != '.')
        score = strtol(fields[5], NULL, 10);

      strand_ch = fields[6][0];

      //  Extract gene name from attributes
      name_buf[0] = '\0';
      if (ftype == FEAT_GENE)
        { if (extract_attr(fields[8], "Name=", name_buf, 1024) == NULL)
            extract_attr(fields[8], "gene=", name_buf, 1024);
        }

      //  Grow array if needed
      if (n >= cap)
        { cap = cap * 2;
          feats = (GenomeFeature *) Realloc(feats, sizeof(GenomeFeature) * cap,
                                            "Expanding features");
          if (feats == NULL)
            { gzclose(fp);
              return (1);
            }
        }

      feats[n].beg    = scaf_to_abs(gdb, scaf_idx, start);
      feats[n].end    = scaf_to_abs(gdb, scaf_idx, end);
      feats[n].type   = ftype;
      feats[n].strand = (strand_ch == '+') ? 0 : (strand_ch == '-') ? 1 : 2;
      feats[n].score  = score;

      if (name_buf[0] != '\0')
        feats[n].label = add_label(track, name_buf);
      else
        feats[n].label = -1;

      n += 1;
    }

  gzclose(fp);

  track->nfeat    = n;
  track->features = feats;

  qsort(feats, n, sizeof(GenomeFeature), feat_cmp);

  if (build_soff(track, gdb))
    return (1);

  return (0);
}


/*******************************************************************************************
 *
 *  TRACK QUERIES
 *
 ********************************************************************************************/

  //  Binary search for the first feature with beg >= target

static int bsearch_first(GenomeFeature *feats, int lo, int hi, int64 target)
{ while (lo < hi)
    { int mid = lo + (hi - lo) / 2;
      if (feats[mid].beg < target)
        lo = mid + 1;
      else
        hi = mid;
    }
  return (lo);
}

  //  But we also need features that start before 'beg' but extend past it,
  //  so scan backwards from the found position to include overlapping features.

int Query_Track(AnnotTrack *track, GDB *gdb, int64 beg, int64 end,
                GenomeFeature **out)
{ int s, first_s, last_s;
  int lo, hi, start_idx, end_idx;
  GDB_SCAFFOLD *scaffolds;
  GDB_CONTIG   *contigs;

  if (track == NULL || track->nfeat == 0)
    { *out = NULL;
      return (0);
    }

  scaffolds = gdb->scaffolds;
  contigs   = gdb->contigs;

  //  Find range of scaffolds that overlap [beg, end)
  first_s = -1;
  last_s  = -1;
  for (s = 0; s < track->nscaff; s++)
    { int64 scaf_start = contigs[scaffolds[s].fctg].sbeg;
      int64 scaf_end   = scaf_start + scaffolds[s].slen;
      if (scaf_end <= beg)
        continue;
      if (scaf_start >= end)
        break;
      if (first_s < 0)
        first_s = s;
      last_s = s;
    }

  if (first_s < 0)
    { *out = NULL;
      return (0);
    }

  //  Get feature index range across all matching scaffolds
  lo = (int) track->soff[first_s];
  hi = (int) track->soff[last_s + 1];

  //  Binary search for first feature that could overlap
  start_idx = bsearch_first(track->features, lo, hi, beg);

  //  Scan backwards to include features that start before beg but overlap it.
  //  Must scan past short features (exons) to find spanning genes behind them.
  { int scan = start_idx;
    while (scan > lo)
      { scan--;
        if (track->features[scan].end > beg)
          start_idx = scan;
        //  Stop scanning once we're far enough back that no feature
        //  could possibly span to beg (heuristic: 500 features back)
        if (start_idx - scan > 500)
          break;
      }
  }

  //  Find end: features starting at or past 'end' can't overlap
  end_idx = start_idx;
  while (end_idx < hi && track->features[end_idx].beg < end)
    end_idx++;

  *out = track->features + start_idx;
  return (end_idx - start_idx);
}


/*******************************************************************************************
 *
 *  CLEANUP
 *
 ********************************************************************************************/

void Free_AnnotTrack(AnnotTrack *track)
{ if (track->features != NULL)
    free(track->features);
  if (track->soff != NULL)
    free(track->soff);
  if (track->labels != NULL)
    free(track->labels);
  track->features = NULL;
  track->soff     = NULL;
  track->labels   = NULL;
  track->nfeat    = 0;
}
