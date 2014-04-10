#!/bin/sh

RUNTIME_DERIVED_SOURCES_DIR=${BUILT_PRODUCTS_DIR}/DerivedSources/JavaScriptCoreRuntime
RUNTIME_INSTALL_DIR=${BUILT_PRODUCTS_DIR}/${JAVASCRIPTCORE_RESOURCES_DIR}/Runtime

for arch in $ARCHS;
do
    if [ -d "$RUNTIME_DERIVED_SOURCES_DIR/$arch" ];
    then
        mkdir -p "$RUNTIME_INSTALL_DIR/$arch"
        cp "$RUNTIME_DERIVED_SOURCES_DIR/$arch"/*.bc.gz "$RUNTIME_INSTALL_DIR/$arch"/.
        ${SRCROOT}/build-symbol-table-index.py $arch
    fi
done
