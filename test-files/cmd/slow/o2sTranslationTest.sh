#!/bin/bash
set -e

# Test reading and writeing o2s_X files
TRANS_TDS40=$HOOT_HOME/translations/TDSv40.js
TRANS_TDS61=$HOOT_HOME/translations/TDSv61.js
TRANS_MGCP=$HOOT_HOME/translations/MGCP_TRD4.js
TRANS_GGDM=$HOOT_HOME/translations/GGDMv30.js

inputFile=test-files/o2s_test.osm
tds40File=test-files/o2s_tds40.osm
ggdmFile=test-files/o2s_ggdm.osm

outputDir=test-output/o2s
mkdir -p $outputDir
rm -rf $outputDir/*

# Normal Hoot options
HOOT_OPT="--info"

# Hoot options for debugging the test input and output
# NOTE: This will generate HEAPS of output.
#HOOT_OPT="--info -D ogr.debug.dumptags=true -D ogr.debug.lookupcolumn=true -D ogr.debug.lookupclash=true -D ogr.debug.dumpvalidate=true"


##### Start Tests #####
# Export files
echo "### TDSv40 ###"
hoot osm2ogr $HOOT_OPT -D ogr.thematic.structure=false $TRANS_TDS40 $inputFile $outputDir/tds40.shp
hoot ogr2osm $HOOT_OPT $TRANS_TDS40 $outputDir/tds40.osm \
  $outputDir/tds40/HUT_P.shp \
  $outputDir/tds40/o2s_*.shp \
  $outputDir/tds40/ORCHARD_S.shp \
  $outputDir/tds40/RIVER_C.shp

hoot is-match $outputDir/tds40.osm $tds40File || diff $outputDir/tds40.osm $tds40File

echo
echo "### TDSv61 ###"
hoot osm2ogr $HOOT_OPT -D ogr.thematic.structure=false $TRANS_TDS61 $inputFile $outputDir/tds61.shp
hoot ogr2osm $HOOT_OPT $TRANS_TDS61 $outputDir/tds61.osm \
  $outputDir/tds61/HUT_P.shp \
  $outputDir/tds61/o2s_*.shp \
  $outputDir/tds61/ORCHARD_S.shp \
  $outputDir/tds61/RIVER_C.shp

hoot is-match $outputDir/tds61.osm $inputFile || diff $outputDir/tds61.osm $inputFile

echo
echo "### MGCP ###"
hoot osm2ogr $HOOT_OPT $TRANS_MGCP $inputFile $outputDir/mgcp.shp
hoot ogr2osm $HOOT_OPT $TRANS_MGCP $outputDir/mgcp.osm \
  $outputDir/mgcp/PAL099.shp \
  $outputDir/mgcp/o2s_*.shp \
  $outputDir/mgcp/AEA040.shp \
  $outputDir/mgcp/LBH140.shp

hoot is-match $outputDir/mgcp.osm $inputFile || diff $outputDir/mgcp.osm $inputFile

echo
echo "### GGDM ###"
hoot osm2ogr $HOOT_OPT -D ogr.thematic.structure=false $TRANS_GGDM $inputFile $outputDir/ggdm.shp
hoot ogr2osm $HOOT_OPT $TRANS_GGDM $outputDir/ggdm.osm \
  $outputDir/ggdm/HUT_P.shp \
  $outputDir/ggdm/o2s_*.shp \
  $outputDir/ggdm/ORCHARD_S.shp \
  $outputDir/ggdm/RIVER_C.shp

hoot is-match $outputDir/ggdm.osm $ggdmFile || diff $outputDir/ggdm.osm $ggdmFile
