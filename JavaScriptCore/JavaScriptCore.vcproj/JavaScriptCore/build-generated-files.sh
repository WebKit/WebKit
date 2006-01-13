#!/usr/bin/bash

# Convert the DOS WebKitOutputDir variable to a unix path.
WebKitUnixDir=`cygpath -a -u "$2"`

# Set up the directory that will hold all our generated files.
DerivedSourcesDir="$WebKitUnixDir/JavaScriptCore.intermediate/$1/JavaScriptCore.intermediate/DerivedSources"
mkdir -p "$DerivedSourcesDir"

# Invoke the create_hash_table perl script to create all of our lookup tables

if [ ../../kjs/array_object.cpp -nt "$DerivedSourcesDir/array_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/array_object.cpp > "$DerivedSourcesDir/array_object.lut.h" -i
fi

if [ ../../kjs/bool_object.cpp -nt "$DerivedSourcesDir/bool_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/bool_object.cpp > "$DerivedSourcesDir/bool_object.lut.h" -i
fi

if [ ../../kjs/date_object.cpp -nt "$DerivedSourcesDir/date_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/date_object.cpp > "$DerivedSourcesDir/date_object.lut.h" -i
fi

if [ ../../kjs/error_object.cpp -nt "$DerivedSourcesDir/error_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/error_object.cpp > "$DerivedSourcesDir/error_object.lut.h" -i
fi

if [ ../../kjs/function_object.cpp -nt "$DerivedSourcesDir/function_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/function_object.cpp > "$DerivedSourcesDir/function_object.lut.h" -i
fi

if [ ../../kjs/math_object.cpp -nt "$DerivedSourcesDir/math_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/math_object.cpp > "$DerivedSourcesDir/math_object.lut.h" -i
fi

if [ ../../kjs/number_object.cpp -nt "$DerivedSourcesDir/number_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/number_object.cpp > "$DerivedSourcesDir/number_object.lut.h" -i
fi

if [ ../../kjs/object_object.cpp -nt "$DerivedSourcesDir/object_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/object_object.cpp > "$DerivedSourcesDir/object_object.lut.h" -i
fi

if [ ../../kjs/regexp_object.cpp -nt "$DerivedSourcesDir/regexp_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/regexp_object.cpp > "$DerivedSourcesDir/regexp_object.lut.h" -i
fi

if [ ../../kjs/string_object.cpp -nt "$DerivedSourcesDir/string_object.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/string_object.cpp > "$DerivedSourcesDir/string_object.lut.h" -i
fi

if [ ../../kjs/string_object.cpp -nt "$DerivedSourcesDir/lexer.lut.h" ]; then
  ../../kjs/create_hash_table ../../kjs/keywords.table > "$DerivedSourcesDir/lexer.lut.h" -i
fi

# Generate the grammar using bison
if [ ../../kjs/grammar.y -nt "$DerivedSourcesDir/grammar.cpp" ]; then
  echo "Generating the JS grammar using bison..."
  bison -d -p kjsyy ../../kjs/grammar.y -o "$DerivedSourcesDir/grammar.cpp"
  mv "$DerivedSourcesDir/grammar.hpp" "$DerivedSourcesDir/grammar.h"
fi
