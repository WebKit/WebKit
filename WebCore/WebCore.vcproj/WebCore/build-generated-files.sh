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

if [ khtml/html/doctypes.gperf -nt "$DerivedSourcesDir/doctypes.cpp" ]; then
  echo "Generating doctype modes table..."
  gperf -CEot -L ANSI-C -k '*' -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards khtml/html/doctypes.gperf > "$DerivedSourcesDir/doctypes.cpp"
fi

# Generate the CSS tokenizer using flex
if [ khtml/css/tokenizer.flex -nt "$DerivedSourcesDir/tokenizer.cpp" ]; then
  echo "Generating the CSS tokenizer using flex..."
  flex -t khtml/css/tokenizer.flex | perl khtml/css/maketokenizer > "$DerivedSourcesDir/tokenizer.cpp"
fi

# Generate the CSS grammar using bison
if [ khtml/css/css_grammar.y -nt "$DerivedSourcesDir/css_grammar.cpp" ]; then
  echo "Generating the CSS grammar using bison..."
  bison -d -p cssyy khtml/css/css_grammar.y -o "$DerivedSourcesDir/css_grammar.cpp"
  mv "$DerivedSourcesDir/css_grammar.hpp" "$DerivedSourcesDir/css_grammar.h"
fi

# Generate the lookup tables for the JS bindings

if [ khtml/ecma/domparser.cpp -nt "$DerivedSourcesDir/domparser.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/domparser.cpp > "$DerivedSourcesDir/domparser.lut.h"
fi

if [ khtml/ecma/kjs_css.cpp -nt "$DerivedSourcesDir/kjs_css.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_css.cpp > "$DerivedSourcesDir/kjs_css.lut.h"
fi

if [ khtml/ecma/kjs_dom.cpp -nt "$DerivedSourcesDir/kjs_dom.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_dom.cpp > "$DerivedSourcesDir/kjs_dom.lut.h"
fi

if [ khtml/ecma/kjs_events.cpp -nt "$DerivedSourcesDir/kjs_events.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_events.cpp > "$DerivedSourcesDir/kjs_events.lut.h"
fi

if [ khtml/ecma/kjs_html.cpp -nt "$DerivedSourcesDir/kjs_html.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_html.cpp > "$DerivedSourcesDir/kjs_html.lut.h"
fi

if [ khtml/ecma/kjs_navigator.cpp -nt "$DerivedSourcesDir/kjs_navigator.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_navigator.cpp > "$DerivedSourcesDir/kjs_navigator.lut.h"
fi

if [ khtml/ecma/kjs_range.cpp -nt "$DerivedSourcesDir/kjs_range.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_range.cpp > "$DerivedSourcesDir/kjs_range.lut.h"
fi

if [ khtml/ecma/kjs_traversal.cpp -nt "$DerivedSourcesDir/kjs_traversal.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_traversal.cpp > "$DerivedSourcesDir/kjs_traversal.lut.h"
fi

if [ khtml/ecma/kjs_views.cpp -nt "$DerivedSourcesDir/kjs_views.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_views.cpp > "$DerivedSourcesDir/kjs_views.lut.h"
fi

if [ khtml/ecma/kjs_window.cpp -nt "$DerivedSourcesDir/kjs_window.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_window.cpp > "$DerivedSourcesDir/kjs_window.lut.h"
fi

if [ khtml/ecma/xmlhttprequest.cpp -nt "$DerivedSourcesDir/xmlhttprequest.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/xmlhttprequest.cpp > "$DerivedSourcesDir/xmlhttprequest.lut.h"
fi

if [ khtml/ecma/xmlserializer.cpp -nt "$DerivedSourcesDir/xmlserializer.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/xmlserializer.cpp > "$DerivedSourcesDir/xmlserializer.lut.h"
fi

if [ khtml/ecma/XSLTProcessor.cpp -nt "$DerivedSourcesDir/XSLTProcessor.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/XSLTProcessor.cpp > "$DerivedSourcesDir/XSLTProcessor.lut.h"
fi

