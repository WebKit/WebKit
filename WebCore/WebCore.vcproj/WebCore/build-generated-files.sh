#!/usr/bin/bash

# Convert the DOS WebKitOutputDir variable to a unix path.
WebKitUnixDir=`cygpath -a -u "$2"`

# Set up the directory that will hold all our generated files.
DerivedSourcesDir="$WebKitUnixDir/WebCore.intermediate/$1/WebCore.intermediate/DerivedSources"
mkdir -p "$DerivedSourcesDir"

# Rebuild cssproperties.h and cssvalues.h if the .in files have changed
cd ../..
WebCoreSourceRoot=`pwd`
if [ khtml/css/cssproperties.in -nt "$DerivedSourcesDir/cssproperties.h" ]; then
echo "Generating CSS properties table..."
cat khtml/css/cssproperties.in > "$DerivedSourcesDir/cssproperties.in"
cd "$DerivedSourcesDir"
sh "$WebCoreSourceRoot/khtml/css/makeprop"
fi

cd "$WebCoreSourceRoot"
if [ khtml/css/cssvalues.in -nt "$DerivedSourcesDir/cssvalues.h" ]; then
echo "Generating CSS values table..."
cat khtml/css/cssvalues.in > "$DerivedSourcesDir/cssvalues.in"
cd "$DerivedSourcesDir"
sh "$WebCoreSourceRoot/khtml/css/makevalues"
fi

cd "$WebCoreSourceRoot"
if [ khtml/html/kentities.gperf -nt "$DerivedSourcesDir/kentities.c" ]; then
echo "Generating entities table..."
gperf -a -L ANSI-C -C -G -c -o -t -k '*' -NfindEntity -D -s 2 khtml/html/kentities.gperf > "$DerivedSourcesDir/kentities.c"
fi

cd "$WebCoreSourceRoot"
if [ khtml/html/doctypes.gperf -nt "$DerivedSourcesDir/doctypes.cpp" ]; then
echo "Generating doctype modes table..."
gperf -CEot -L ANSI-C -k '*' -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards khtml/html/doctypes.gperf > "$DerivedSourcesDir/doctypes.cpp"
fi


		