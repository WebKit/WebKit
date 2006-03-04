#!/usr/bin/bash

# Convert the DOS WebKitOutputDir variable to a unix path.
WebKitUnixDir=`cygpath -a -u "$2"`

# Set up the directory that will hold all our generated files.
DerivedSourcesDir="$WebKitUnixDir/WebCore.intermediate/$1/WebCore.intermediate/DerivedSources"
mkdir -p "$DerivedSourcesDir"

# Rebuild cssproperties.h and cssvalues.h if the .in files have changed
cd ../..
WebCoreSourceRoot=`pwd`
if [ css/cssproperties.in -nt "$DerivedSourcesDir/cssproperties.h" ]; then
  echo "Generating CSS properties table..."
  cat css/cssproperties.in > "$DerivedSourcesDir/cssproperties.in"
  cd "$DerivedSourcesDir"
  sh "$WebCoreSourceRoot/css/makeprop"
fi

cd "$WebCoreSourceRoot"
  if [ css/cssvalues.in -nt "$DerivedSourcesDir/cssvalues.h" ]; then
  echo "Generating CSS values table..."
  cat css/cssvalues.in > "$DerivedSourcesDir/cssvalues.in"
  cd "$DerivedSourcesDir"
  sh "$WebCoreSourceRoot/css/makevalues"
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
if [ css/tokenizer.flex -nt "$DerivedSourcesDir/tokenizer.cpp" ]; then
  echo "Generating the CSS tokenizer using flex..."
  flex -t css/tokenizer.flex | perl css/maketokenizer > "$DerivedSourcesDir/tokenizer.cpp"
fi

# Generate the CSS grammar using bison
if [ css/css_grammar.y -nt "$DerivedSourcesDir/css_grammar.cpp" ]; then
  echo "Generating the CSS grammar using bison..."
  bison -d -p cssyy css/css_grammar.y -o "$DerivedSourcesDir/css_grammar.cpp"
  mv "$DerivedSourcesDir/css_grammar.hpp" "$DerivedSourcesDir/css_grammar.h"
fi

if [ css/make-css-file-arrays.pl -nt "$DerivedSourcesDir/UserAgentStyleSheets.h" -o css/html4.css -nt "$DerivedSourcesDir/UserAgentStyleSheets.h" -o css/quirks.css -nt "$DerivedSourcesDir/UserAgentStyleSheets.h" -o css/svg.css -nt "$DerivedSourcesDir/UserAgentStyleSheets.h" ]; then
  echo "Re-generating the user agent stylesheet files..."
  css/make-css-file-arrays.pl "$DerivedSourcesDir/UserAgentStyleSheets.h" "$DerivedSourcesDir/UserAgentStyleSheetsData.cpp" css/html4.css css/quirks.css css/svg.css
  touch css/UserAgentStyleSheets.cpp
fi

# Generate the color data table.
if [ platform/ColorData.gperf -nt "$DerivedSourcesDir/ColorData.c" ]; then
  echo "Generating the color table data for the Color class..."
  gperf -CDEot -L 'ANSI-C' -k '*' -N findColor platform/ColorData.gperf > "$DerivedSourcesDir/ColorData.c"
fi

# Generate the charset name table
if [ platform/character-sets.txt -nt "$DerivedSourcesDir/CharsetData.cpp" -o platform/win/win-encodings.txt -nt "$DerivedSourcesDir/CharsetData.cpp" -o platform/make-charset-table.pl -nt "$DerivedSourcesDir/CharsetData.cpp" ]; then
  echo "Generating character set name table..."
  platform/make-charset-table.pl platform/character-sets.txt platform/win/win-encodings.txt "" > "$DerivedSourcesDir/CharsetData.cpp"
fi

# Generate the lookup tables for the JS bindings

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/domparser.lut.h" -o khtml/ecma/domparser.cpp -nt "$DerivedSourcesDir/domparser.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/domparser.cpp > "$DerivedSourcesDir/domparser.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/kjs_css.lut.h" -o khtml/ecma/kjs_css.cpp -nt "$DerivedSourcesDir/kjs_css.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_css.cpp > "$DerivedSourcesDir/kjs_css.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/kjs_dom.lut.h" -o khtml/ecma/kjs_dom.cpp -nt "$DerivedSourcesDir/kjs_dom.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_dom.cpp > "$DerivedSourcesDir/kjs_dom.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/kjs_events.lut.h" -o khtml/ecma/kjs_events.cpp -nt "$DerivedSourcesDir/kjs_events.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_events.cpp > "$DerivedSourcesDir/kjs_events.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/kjs_html.lut.h" -o khtml/ecma/kjs_html.cpp -nt "$DerivedSourcesDir/kjs_html.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_html.cpp > "$DerivedSourcesDir/kjs_html.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/kjs_navigator.lut.h" -o khtml/ecma/kjs_navigator.cpp -nt "$DerivedSourcesDir/kjs_navigator.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_navigator.cpp > "$DerivedSourcesDir/kjs_navigator.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/kjs_range.lut.h" -o khtml/ecma/kjs_range.cpp -nt "$DerivedSourcesDir/kjs_range.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_range.cpp > "$DerivedSourcesDir/kjs_range.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/kjs_traversal.lut.h" -o khtml/ecma/kjs_traversal.cpp -nt "$DerivedSourcesDir/kjs_traversal.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_traversal.cpp > "$DerivedSourcesDir/kjs_traversal.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/kjs_views.lut.h" -o khtml/ecma/kjs_views.cpp -nt "$DerivedSourcesDir/kjs_views.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_views.cpp > "$DerivedSourcesDir/kjs_views.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/kjs_window.lut.h" -o khtml/ecma/kjs_window.cpp -nt "$DerivedSourcesDir/kjs_window.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/kjs_window.cpp > "$DerivedSourcesDir/kjs_window.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/xmlhttprequest.lut.h" -o xml/xmlhttprequest.cpp -nt "$DerivedSourcesDir/xmlhttprequest.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table xml/xmlhttprequest.cpp > "$DerivedSourcesDir/xmlhttprequest.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/xmlserializer.lut.h" -o khtml/ecma/xmlserializer.cpp -nt "$DerivedSourcesDir/xmlserializer.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/xmlserializer.cpp > "$DerivedSourcesDir/xmlserializer.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/XSLTProcessor.lut.h" -o khtml/ecma/XSLTProcessor.cpp -nt "$DerivedSourcesDir/XSLTProcessor.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/XSLTProcessor.cpp > "$DerivedSourcesDir/XSLTProcessor.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/JSXMLHttpRequest.lut.h" -o khtml/ecma/JSXMLHttpRequest.cpp -nt "$DerivedSourcesDir/JSXMLHttpRequest.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/JSXMLHttpRequest.cpp > "$DerivedSourcesDir/JSXMLHttpRequest.lut.h"
fi

WebKitOutputConfigDir="$WebKitUnixDir/$1"
mkdir -p "$WebKitOutputConfigDir"


if [ ../libxml/bin/libxml2.dll -nt "$WebKitOutputConfigDir/libxml2.dll" ]; then
    echo "Copying libxml2.dll"
    cp ../libxml/bin/libxml2.dll "$WebKitOutputConfigDir" || exit 1
fi

if [ ../libxml/bin/libxslt.dll -nt "$WebKitOutputConfigDir/libxslt.dll" ]; then
    echo "Copying libxslt.dll"
    cp ../libxslt/bin/libxslt.dll "$WebKitOutputConfigDir" || exit 1
fi

if [ ../libxml/bin/iconv.dll -nt "$WebKitOutputConfigDir/iconv.dll" ]; then
    echo "Copying iconv.dll"
    cp ../iconv/bin/iconv.dll "$WebKitOutputConfigDir" || exit 1
fi

if [ ../libxml/bin/zlib1.dll -nt "$WebKitOutputConfigDir/zlib1.dll" ]; then
    echo "Copying zlib1.dll"
    cp ../zlib/bin/zlib1.dll "$WebKitOutputConfigDir" || exit 1
fi

# Auto-generate bindings from .idl files
echo "Auto-generating bindings from .idl files for the dom directory..."
perl -I"$WebCoreSourceRoot/bindings/scripts" "$WebCoreSourceRoot/bindings/scripts/generate-bindings.pl" --generator JS --idldir "$WebCoreSourceRoot/dom" --outputdir "$DerivedSourcesDir"
