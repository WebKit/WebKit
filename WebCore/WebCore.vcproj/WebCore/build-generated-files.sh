#!/usr/bin/bash

# Convert the DOS WebKitOutputDir variable to a unix path.
WebKitUnixDir=`cygpath -a -u "$2"`

# Set up the directory that will hold all our generated files.
DerivedSourcesDir="$WebKitUnixDir/WebCore.intermediate/$1/WebCore.intermediate/DerivedSources"
mkdir -p "$DerivedSourcesDir"

# Rebuild CSSPropertyNames.h and CSSValueKeywords.h if the .in files have changed
cd ../..
WebCoreSourceRoot=`pwd`
if [ css/CSSPropertyNames.in -nt "$DerivedSourcesDir/CSSPropertyNames.h" ]; then
  echo "Generating CSS properties table..."
  cat css/CSSPropertyNames.in > "$DerivedSourcesDir/CSSPropertyNames.in"
  cd "$DerivedSourcesDir"
  sh "$WebCoreSourceRoot/css/makeprop"
fi

cd "$WebCoreSourceRoot"
  if [ css/CSSValueKeywords.in -nt "$DerivedSourcesDir/CSSValueKeywords.h" ]; then
  echo "Generating CSS values table..."
  cat css/CSSValueKeywords.in > "$DerivedSourcesDir/CSSValueKeywords.in"
  cd "$DerivedSourcesDir"
  sh "$WebCoreSourceRoot/css/makevalues"
fi

cd "$WebCoreSourceRoot"

if [ khtml/html/HTMLEntityNames.gperf -nt "$DerivedSourcesDir/HTMLEntityNames.c" ]; then
  echo "Generating entities table..."
  gperf -a -L ANSI-C -C -G -c -o -t -k '*' -NfindEntity -D -s 2 khtml/html/HTMLEntityNames.gperf > "$DerivedSourcesDir/HTMLEntityNames.c"
fi

if [ khtml/html/DocTypeStrings.gperf -nt "$DerivedSourcesDir/DocTypeStrings.cpp" ]; then
  echo "Generating doctype modes table..."
  gperf -CEot -L ANSI-C -k '*' -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards khtml/html/DocTypeStrings.gperf > "$DerivedSourcesDir/DocTypeStrings.cpp"
fi

# Generate the CSS tokenizer using flex
if [ css/tokenizer.flex -nt "$DerivedSourcesDir/tokenizer.cpp" ]; then
  echo "Generating the CSS tokenizer using flex..."
  flex -t css/tokenizer.flex | perl css/maketokenizer > "$DerivedSourcesDir/tokenizer.cpp"
fi

# Generate the CSS grammar using bison
if [ css/CSSGrammar.y -nt "$DerivedSourcesDir/CSSGrammar.cpp" ]; then
  echo "Generating the CSS grammar using bison..."
  bison -d -p cssyy css/CSSGrammar.y -o "$DerivedSourcesDir/CSSGrammar.cpp"
  mv "$DerivedSourcesDir/CSSGrammar.hpp" "$DerivedSourcesDir/CSSGrammar.h"
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

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/JSDOMParser.lut.h" -o khtml/ecma/JSDOMParser.cpp -nt "$DerivedSourcesDir/JSDOMParser.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/JSDOMParser.cpp > "$DerivedSourcesDir/JSDOMParser.lut.h"
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

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/JSXMLSerializer.lut.h" -o khtml/ecma/JSXMLSerializer.cpp -nt "$DerivedSourcesDir/JSXMLSerializer.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/JSXMLSerializer.cpp > "$DerivedSourcesDir/JSXMLSerializer.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/JSXSLTProcessor.lut.h" -o khtml/ecma/JSXSLTProcessor.cpp -nt "$DerivedSourcesDir/JSXSLTProcessor.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/JSXSLTProcessor.cpp > "$DerivedSourcesDir/JSXSLTProcessor.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/JSXMLHttpRequest.lut.h" -o khtml/ecma/JSXMLHttpRequest.cpp -nt "$DerivedSourcesDir/JSXMLHttpRequest.lut.h" ]; then
  ../JavaScriptCore/kjs/create_hash_table khtml/ecma/JSXMLHttpRequest.cpp > "$DerivedSourcesDir/JSXMLHttpRequest.lut.h"
fi

if [ ../JavaScriptCore/kjs/create_hash_table -nt "$DerivedSourcesDir/JSCanvasRenderingContext2DBaseTable.cpp" -o bindings/js/JSCanvasRenderingContext2DBase.cpp -nt "$DerivedSourcesDir/JSCanvasRenderingContext2DBaseTable.cpp" ]; then
  ../JavaScriptCore/kjs/create_hash_table bindings/js/JSCanvasRenderingContext2DBase.cpp > "$DerivedSourcesDir/JSCanvasRenderingContext2DBaseTable.cpp"
fi

WebKitOutputConfigDir="$WebKitUnixDir/$1"
mkdir -p "$WebKitOutputConfigDir"


if [ ../libxml/bin/libxml2.dll -nt "$WebKitOutputConfigDir/libxml2.dll" ]; then
    echo "Copying libxml2.dll"
    cp ../libxml/bin/libxml2.dll "$WebKitOutputConfigDir" || exit 1
fi

if [ ../libxslt/bin/libxslt.dll -nt "$WebKitOutputConfigDir/libxslt.dll" ]; then
    echo "Copying libxslt.dll"
    cp ../libxslt/bin/libxslt.dll "$WebKitOutputConfigDir" || exit 1
fi

if [ ../iconv/bin/iconv.dll -nt "$WebKitOutputConfigDir/iconv.dll" ]; then
    echo "Copying iconv.dll"
    cp ../iconv/bin/iconv.dll "$WebKitOutputConfigDir" || exit 1
fi

if [ ../zlib/bin/zlib1.dll -nt "$WebKitOutputConfigDir/zlib1.dll" ]; then
    echo "Copying zlib1.dll"
    cp ../zlib/bin/zlib1.dll "$WebKitOutputConfigDir" || exit 1
fi

# Auto-generate bindings from .idl files
echo "Auto-generating bindings from .idl files"
perl -I"$WebCoreSourceRoot/bindings/scripts" "$WebCoreSourceRoot/bindings/scripts/generate-bindings.pl" --generator JS --idldir "$WebCoreSourceRoot/dom" --outputdir "$DerivedSourcesDir"
perl -I"$WebCoreSourceRoot/bindings/scripts" "$WebCoreSourceRoot/bindings/scripts/generate-bindings.pl" --generator JS --idldir "$WebCoreSourceRoot/html" --outputdir "$DerivedSourcesDir"
