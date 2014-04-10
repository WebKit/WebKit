#!/bin/sh

OBJ_DIR=${TARGET_TEMP_DIR}/Objects-${CURRENT_VARIANT}
RUNTIME_DERIVED_SOURCES_DIR=${BUILT_PRODUCTS_DIR}/DerivedSources/JavaScriptCoreRuntime

for arch in $ARCHS;
do
    if [ -d "$OBJ_DIR/$arch" ];
    then
        mkdir -p "$RUNTIME_DERIVED_SOURCES_DIR/$arch"
        for file in "$OBJ_DIR/$arch"/*.o;
        do
            file_name=${file##*/}
            gzip -9 -c "$file" > "$RUNTIME_DERIVED_SOURCES_DIR/$arch/${file_name%.o}.bc.gz"
        done
    fi
done
