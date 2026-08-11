#!/usr/bin/env perl
#
# Copyright (C) 2017 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Unit tests of VCSUtils::fixSVNPatchForAdditionWithHistory().

use strict;
use warnings;

use Test::More;
use VCSUtils;

my @testCaseHashRefs = (
###
# Test cases that should have no change
##
{ # New test
    diffName => "fixSVNPatchForAdditionWithHistory: [no change] modify file",
    inputText => <<'END',
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)
@@ -1 +1 @@
-A
+A2
END
    expectedReturn => <<'END'
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)
@@ -1 +1 @@
-A
+A2
END
},
{ # New test
    diffName => "fixSVNPatchForAdditionWithHistory: [no change] delete file",
    inputText => <<'END',
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(nonexistent)
@@ -1 +0,0 @@
-A
END
    expectedReturn => <<'END'
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(nonexistent)
@@ -1 +0,0 @@
-A
END
},
{ # New test
    diffName => "fixSVNPatchForAdditionWithHistory: [no change] add svn:executable property",
    inputText => <<'END',
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Added: svn:executable
## -0,0 +1 ##
+*
\ No newline at end of property
END
    expectedReturn => <<'END'
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Added: svn:executable
## -0,0 +1 ##
+*
\ No newline at end of property
END
},
{ # New test
    diffName => "fixSVNPatchForAdditionWithHistory: [no change] remove svn:executable property",
    inputText => <<'END',
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Deleted: svn:executable
## -1 +0,0 ##
-*
\ No newline at end of property
END
    expectedReturn => <<'END'
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Deleted: svn:executable
## -1 +0,0 ##
-*
\ No newline at end of property
END
},
###
# Moved/copied file using SVN 1.9 syntax
##
{ # New test
    diffName => "fixSVNPatchForAdditionWithHistory: moved/copied file",
    inputText => <<'END',
Index: test_file.txt
===================================================================
END
    expectedReturn => ""
},
{ # New test
    diffName => "fixSVNPatchForAdditionWithHistory: moved/copied file with added svn:executable property",
    inputText => <<'END',
Index: test_file.txt
===================================================================
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Added: svn:executable
## -0,0 +1 ##
+*
\ No newline at end of property
END
    expectedReturn => <<'END'
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Added: svn:executable
## -0,0 +1 ##
+*
\ No newline at end of property
END
},
{ # New test
    diffName => "fixSVNPatchForAdditionWithHistory: moved/copied file with removed svn:executable property",
    inputText => <<'END',
Index: test_file.txt
===================================================================
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Deleted: svn:executable
## -1 +0,0 ##
-*
\ No newline at end of property
END
    expectedReturn => <<'END'
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Deleted: svn:executable
## -1 +0,0 ##
-*
\ No newline at end of property
END
},
{ # New test
    diffName => "fixSVNPatchForAdditionWithHistory: moved/copied file with added multi-line property",
    inputText => <<'END',
Index: test_file.txt
===================================================================
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Added: documentation
## -0,0 +1,3 ##
+A
+long sentence that spans
+multiple lines.
END
    expectedReturn => <<'END'
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Added: documentation
## -0,0 +1,3 ##
+A
+long sentence that spans
+multiple lines.
END
},
{ # New test
    diffName => "fixSVNPatchForAdditionWithHistory: moved/copied file with modified multi-line property",
    inputText => <<'END',
Index: test_file.txt
===================================================================
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Modified: documentation
## -1,3 +1,3 ##
-A
+Another
 long sentence that spans
 multiple lines.
END
    expectedReturn => <<'END'
Index: test_file.txt
===================================================================
--- test_file.txt	(revision 1)
+++ test_file.txt	(working copy)

Property changes on: test_file.txt
___________________________________________________________________
Modified: documentation
## -1,3 +1,3 ##
-A
+Another
 long sentence that spans
 multiple lines.
END
},
);

my $testCasesCount = @testCaseHashRefs;
plan(tests => $testCasesCount); # Total number of assertions.

foreach my $testCase (@testCaseHashRefs) {
    my $testNameStart = "fixSVNPatchForAdditionWithHistory(): $testCase->{diffName}: comparing";

    my $got = VCSUtils::fixSVNPatchForAdditionWithHistory($testCase->{inputText});
    my $expectedReturn = $testCase->{expectedReturn};
 
    is_deeply($got, $expectedReturn, "$testNameStart return value.");
}
