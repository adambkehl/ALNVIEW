#!/bin/bash
# Download CHM13 v2.0 annotation files for ALNview
#
# Usage: ./scripts/download_chm13v2_annotations.sh [output_dir]
#
# Downloads gene annotations, centromere/satellite, cytobands, telomeres,
# segmental duplications, genome features, and repeat annotations from the
# T2T CHM13 v2.0 annotation repository.

set -euo pipefail

BASE_URL="https://s3-us-west-2.amazonaws.com/human-pangenomics/T2T/CHM13/assemblies/annotation"
OUT_DIR="${1:-annotations/chm13v2.0}"

mkdir -p "$OUT_DIR"

FILES=(
  "chm13v2.0_RefSeq_Liftoff_v5.2.gff3.gz"
  "chm13v2.0_censat_v2.1.bed"
  "chm13v2.0_cytobands_allchrs.bed"
  "chm13v2.0_telomere.bed"
  "chm13v2.0_GenomeFeature_v1.0.bed"
  "chm13v2.0_SD.bed"
  "chm13v2.0_composite-repeats_2022DEC.bed"
  "chm13v2.0_new-satellites_2022DEC.bed"
)

for f in "${FILES[@]}"; do
  if [ -f "$OUT_DIR/$f" ]; then
    echo "Already exists: $OUT_DIR/$f"
  else
    echo "Downloading: $f"
    curl -fSL "$BASE_URL/$f" -o "$OUT_DIR/$f"
  fi
done

echo "Done. Files saved to $OUT_DIR/"
