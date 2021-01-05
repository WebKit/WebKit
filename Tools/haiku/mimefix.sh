#!/bin/sh
# This script fixes some known issues with MIME type detection in Haiku. The MIME sniffer does not
# always make the best guess on the type of files, and the test suite relies on the types being
# correct. So, let's fix them!

set TESTS=BEOS:TYPE LayoutTests/webgl/1.0.3/resources/webgl_test_files/conformance/*/*.html LayoutTests/css3/filters/regions-expanding.html LayoutTests/editing/caret/*.html

rmattr -f BEOS:TYPE $TESTS
addattr BEOS:TYPE $TESTS
