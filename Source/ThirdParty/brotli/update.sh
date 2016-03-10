#!/bin/sh

# Script to update the webkit in-tree copy of the Brotli decompressor.
# Run this within the /Source/ThirdParty/brotli directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t brotli_update.XXXXXX` || exit 1

git clone https://github.com/google/brotli ${MY_TEMP_DIR}/brotli

COMMIT=`(cd ${MY_TEMP_DIR}/brotli && git log | head -n 1)`
perl -p -i -e "s/\[commit [0-9a-f]{40}\]/[${COMMIT}]/" README.webkit;

rm -rf dec
mv ${MY_TEMP_DIR}/brotli/dec dec
rm -rf ${MY_TEMP_DIR}

echo "###"
echo "### Updated brotli/dec to $COMMIT."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
