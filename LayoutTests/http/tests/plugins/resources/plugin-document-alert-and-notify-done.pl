#!/usr/bin/perl -wT
use strict;

# Do not rename this file. The TestNetscapePlugin is hardcoded to look for this filename
# to trigger a JavaScript alert and call testRunner.notifyDone().
print "Content-Type: application/x-webkit-test-netscape\n\n";
print "This is a mock plugin. It alerts when loaded and calls testRunner.notifyDone()";
