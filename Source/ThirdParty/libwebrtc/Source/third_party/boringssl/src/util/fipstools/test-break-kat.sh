# Copyright (c) 2022, Google Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# This script attempts to break each of the known KATs and checks that doing so
# seems to work and at least mentions the correct KAT in the output.

set -x
set -e

TEST_FIPS_BIN="build/util/fipstools/test_fips"

if [ ! -f $TEST_FIPS_BIN ]; then
  echo "$TEST_FIPS_BIN is missing. Run this script from the top level of a"
  echo "BoringSSL checkout and ensure that ./build-fips-break-test-binaries.sh"
  echo "has been run first."
  exit 1
fi

KATS=$(go run util/fipstools/break-kat.go --list-tests)

for kat in $KATS; do
  go run util/fipstools/break-kat.go $TEST_FIPS_BIN $kat > break-kat-bin
  chmod u+x ./break-kat-bin
  if ! (./break-kat-bin 2>&1 >/dev/null || true) | \
       egrep -q "^$kat[^a-zA-Z0-9]"; then
    echo "Failure for $kat did not mention that name in the output"
    exit 1
  fi
  rm ./break-kat-bin
done
