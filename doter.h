#ifndef DOTER
#define DOTER

#include "GDB.h"
#include "sticks.h"

#define MAX_DOTPLOT 10000000
#define MAX_KMERS     750000
#define SUPER_DENSE        7

void *dotplot_memory();

typedef struct 
  { uint64 code;
    int    pos; 
  } Tuple;

typedef struct
  { int    brun;
    Tuple *blist;
    int   *aplot;
  } Dots;

Dots *dotplot(DotPlot *plot, int kmer, View *view, double xa, double ya);

#endif
