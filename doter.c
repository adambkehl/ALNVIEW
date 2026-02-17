#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "GDB.h"
#include "doter.h"
#include "sticks.h"

#undef  DEBUG_CHECK
#undef  DEBUG_STATS
#undef  DEBUG_SHOW
#undef  DEBUG_FILL

typedef struct
  { int    kmer;
    char  *seq;
    Tuple *list;
    double scale;
    uint64 thr;
    uint64 kmask;
    uint64 cumber[5];
  } Build_State;

  //  Remove all entries < thr and shift down returning new length

static int64 new_threshold(Tuple *list, int64 len, uint64 thr)
{ int i, nen;

  nen = 0;
  for (i = 0; i < len; i++)
    if ((list[i].code & 0xffffu) < thr)
      list[nen++] = list[i];
  return (nen);
}

  //  seq[0..len] holds sequence for pos..pos+len, add Tuple entries for all k-mers in
  //    the sequence staring add at list+idx.  Return new top of tuple list.

static int build_segment(int len, char  *seq, int pos, int idx, Build_State *state)
{ int     kmer   = state->kmer;
  Tuple  *list   = state->list;
  double  scale  = state->scale;
  uint64  kmask  = state->kmask;
  uint64 *cumber = state->cumber;
  uint64  thr    = state->thr;
  int     km1    = kmer-1;

  int    p;
  uint64 u, c, x;

  c = u = 0;
  for (p = 0; p < km1; p++)
    { x = seq[p];
      c = (c << 2) | x;
      u = (u >> 2) | cumber[x];
    }
  seq += km1;
  len -= km1;

  for (p = 0; p < len; p++)
    { x = seq[p];
      c = ((c << 2) | x) & kmask;
      u = (u >> 2) | cumber[x];
      if (u < c)
        { if ((u & 0xffffu) < thr)
            { while (idx >= MAX_KMERS)
                { thr *= .66666;
                  idx = new_threshold(list,idx,thr);
                }
              list[idx].code = u;
              list[idx++].pos = (((int) (scale*pos+22.)) << 1) | 1;
            }
        }
      else
        { if ((c & 0xffffu) < thr)
            { while (idx >= MAX_KMERS)
                { thr *= .66666;
                  idx = new_threshold(list,idx,thr);
                }
              list[idx].code = c;
              list[idx++].pos = (((int) (scale*pos+22.)) << 1);
            }
        }
      pos += 1;
    }

  state->thr = thr;
  return (idx);
}

  //  Remove all duplicate Tuples for list and return new length

static int compress(int len, Tuple *list)
{ int i, j;
  int64 lcode, ncode;
  int   lpos, npos;

  lcode = 0;
  lpos  = -1;
  j = 0;
  for (i = 0; i < len; i++)
    { ncode = list[i].code;
      npos  = list[i].pos;
      if (ncode != lcode || npos != lpos)
        { lcode = ncode;
          lpos  = npos;
          list[j++] = list[i];
        }
    }
  // printf("Compression %.1f%%\n",(100.*j)/len);
  return (j);
}

  //  Map absolute genome coord to contig/pos pair

static void map(GDB *gdb, int64 coord, int *cps, int *pos)
{ GDB_SCAFFOLD *scf;
  GDB_CONTIG   *ctg;
  int           i, s, c;

  scf = gdb->scaffolds;
  ctg = gdb->contigs;

  for (i = 0; i < gdb->nscaff; i++)       //  binary search instead ?
    if (ctg[scf[i].fctg].sbeg > coord)
      break;
  s = i-1;

  for (i = scf[s].fctg; i < scf[s].ectg; i++)
    if (ctg[i].sbeg > coord)
      break;
  c = i-1;

  *cps = c;
  *pos = coord - ctg[c].sbeg;
}

  //  Build Tuple list for gdb[vbeg..vend] contig at a time

static int build_vector(GDB *gdb, int64 vbeg, int64 vend, Build_State *state)
{ GDB_CONTIG *ctg;
  char       *seq;
  int cpb, cpe;
  int beg, end;
  int kmer, len;
  int s, c, x, idx;

  seq  = state->seq;
  kmer = state->kmer;

  map(gdb,vbeg,&cpb,&beg);
  map(gdb,vend,&cpe,&end);

  len = vend - vbeg;
  if (len <= 550000)
    state->thr = 0x10000llu;
  else
    state->thr = 0x10000llu * (500000./len);

  if (kmer == 32)
    state->kmask = 0xffffffffffffffffllu;
  else
    state->kmask = (0x1llu << 2*kmer) - 1;

  state->cumber[0] = (0x3llu << 2*(kmer-1));
  state->cumber[1] = (0x2llu << 2*(kmer-1));
  state->cumber[2] = (0x1llu << 2*(kmer-1));
  state->cumber[3] = 0x0llu;
  state->cumber[4] = 0x0llu;

  ctg = gdb->contigs;
  if (cpb == cpe)
    idx = build_segment(end-beg,Get_Contig_Piece(gdb,cpb,beg,end,NUMERIC,seq),0,0,state);
  else
    { x = ctg[cpb].clen - beg;
      s = ctg[cpb].scaf;
      idx = build_segment(x,Get_Contig_Piece(gdb,cpb,beg,ctg[cpb].clen,NUMERIC,seq),0,0,state);
      for (c = cpb+1; c < cpe; c++)
        { if (ctg[c].scaf == s)
            x += ctg[c].sbeg-(ctg[c-1].sbeg+ctg[c-1].clen);
          idx = build_segment(ctg[c].clen,Get_Contig(gdb,c,NUMERIC,seq),x,idx,state);
          s = ctg[c].scaf;
          x += ctg[c].clen;
        }
      if (ctg[cpe].scaf == s)
        x += ctg[cpe].sbeg-(ctg[cpe-1].sbeg+ctg[cpe-1].clen);
      idx = build_segment(end,Get_Contig_Piece(gdb,c,0,end,NUMERIC,seq),x,idx,state);
    }
  return (idx);
}

static int64 count_hits(int brun, Tuple *blist, int arun, Tuple *alist)
{ int     i, j;
  int     al, bl;
  int     al2, bl2;
  uint64  kb, lc;
  int     na, nb;
  int64   nel;

  bl = brun;
  al = arun;

  lc = alist[al-1].code;
  for (bl2 = bl-1; bl2 >= 0; bl2--)
    if (blist[bl2].code < lc)
      break;
  bl2 += 1;
  for (al2 = al-2; al2 >= 0; al2--)
    if (alist[al2].code < lc)
      break;
  al2 += 1;

  nel = 0;
  i = j = 0;
  while (i < bl2)
    { kb = blist[i].code;
      while (alist[j].code < kb)
        j += 1;

      nb = 1;
      for (i++; blist[i].code == kb; i++)
        nb += 1;
      if (alist[j].code == kb)
        { na = 1;
          for (j++; alist[j].code == kb; j++)
            na += 1;
          nel += na*nb;
        }
    }

  if (i < bl && blist[i].code == lc)
    { nb = 1;
      for (i++; i < bl && blist[i].code == lc; i++)
        nb += 1;
      na = 0;
      while (al2++ < al)
        na += 1;
      nel += na*nb;
    }

  return (nel);
}

static int merge(int brun, Tuple *blist, int arun, Tuple *alist, int64 *pels)
{ int    *aplot;
  int     i, j, k;
  int     al, bl;
  int     al2, bl2;
  int     y;
  uint64  kb, lc;
  int     na, nb;
  int64   nel;

  bl = brun;
  al = arun;

  lc = alist[al-1].code;
  for (bl2 = bl-1; bl2 >= 0; bl2--)
    if (blist[bl2].code < lc)
      break;
  bl2 += 1;
  for (al2 = al-2; al2 >= 0; al2--)
    if (alist[al2].code < lc)
      break;
  al2 += 1;

  aplot = (int *) alist;

  nel = 0;
  y = 0;
  i = j = k = 0;
  while (i < bl2)
    { kb = blist[i].code;
      while (alist[j].code < kb)
        j += 1;

      if (alist[j].code == kb)
        { blist[k].code  = y;
          blist[k++].pos = blist[i++].pos;
          nb = 1;
          while (blist[i].code == kb)
            { blist[k].code  = y;
              blist[k++].pos = blist[i++].pos;
              nb += 1;
            }
          aplot[y++] = alist[j++].pos;
          na = 1;
          while (alist[j].code == kb)
            { aplot[y++] = alist[j++].pos;
              na += 1;
            }
          aplot[y++] = -1;
          nel += na*nb;
        }
      else
        { i += 1;
          while (blist[i].code == kb)
            i += 1;
        }
    }

  if (i < bl && blist[i].code == lc)
    { blist[k].code  = y;
      blist[k++].pos = blist[i++].pos;
      nb = 1;
      while (i < bl && blist[i].code == lc)
        { blist[k].code = y;
          blist[k++].pos = blist[i++].pos;
          nb += 1;
        }
      na = 0;
      while (al2 < al)
        { aplot[y++] = alist[al2++].pos;
          na += 1;
        }
      aplot[y++] = -1;
      nel += na*nb;
    }

  *pels = nel;
  return (k);
}

static int TSORT(const void *l, const void *r)
{ Tuple *x = (Tuple *) l;
  Tuple *y = (Tuple *) r;
  if (x->code < y->code)
    return (-1);
  if (x->code > y->code)
    return (1);
  return (x->pos - y->pos);
}

void *dotplot_memory()
{ return (malloc(sizeof(Tuple)*2*MAX_KMERS + MAX_DOTPLOT*2 + 8 + sizeof(Dots))); }

Dots *dotplot(DotPlot *plot, int kmer, View *view, double xa, double ya)
{ Dots  *dot   = (Dots *) plot->dotmemory;
  Tuple *alist = (Tuple *) (dot+1);
  Tuple *blist = alist + MAX_KMERS;
  char  *seq   = (char *) (blist+MAX_KMERS);
  int   *aplot = (int *) alist;

  Build_State state;

  int    arun, brun;
  int    athr, bthr;
  int64  nel;
  double density;

  int64 vX = view->x;
  int64 vY = view->y;
  int64 vW = view->w;
  int64 vH = view->h;

  state.kmer = kmer;
  state.seq  = seq;

  state.scale = xa;
  state.list  = alist;
  arun = build_vector(&(plot->db1->gdb),vX,vX+vW,&state);
  athr = state.thr;

  state.scale = ya;
  state.list  = blist;
  brun = build_vector(&(plot->db2->gdb),vY,vY+vH,&state);
  bthr = state.thr;

  qsort(alist,arun,sizeof(Tuple),TSORT);
  qsort(blist,brun,sizeof(Tuple),TSORT);

#ifdef DEBUG_CHECK
  { int i;

    for (i = 1; i < arun; i++)
      if (alist[i].code < alist[i-1].code)
        printf("Not sorted\n");

    for (i = 1; i < brun; i++)
      if (blist[i].code < blist[i-1].code)
        printf("Not sorted\n");
  }
#endif

  arun = compress(arun,alist);
  brun = compress(brun,blist);

  while (1)
    { nel = count_hits(brun,blist,arun,alist);
      density = (nel/(xa*vW))/(ya*vH);
      if (density <= 10.)
        break;
      density = sqrt(density/9.);
      athr /= density;
      bthr /= density;
      arun = new_threshold(alist,arun,athr);
      brun = new_threshold(blist,brun,bthr);
    }

  brun = merge(brun,blist,arun,alist,&nel);

#ifdef DEBUG_SHOW
  { int i, j;

    printf("Scan Lines:\n");
    for (i = 0; i < brun; i++)
      { printf(" %5d(%d):",blist[i].pos>>1,(blist[i].pos&0x1));
        j = blist[i].code;
        while (aplot[j] >= 0)
          { printf(" %5d(%d)",aplot[j]>>1,(aplot[j]&0x1));
            j += 1;
          }
        printf("\n");
      }
  }
#endif

  dot->brun    = brun;
  dot->aplot   = aplot;
  dot->blist   = blist;

  return (dot);
}
