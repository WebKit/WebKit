#!/usr/bin/perl -w
#
# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
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

# Unit tests of parseDiff().

use strict;
use warnings;

use Test::More;
use VCSUtils;

my @diffHashRefKeys = ( # The $diffHashRef keys to check.
    "copiedFromPath",
    "indexPath",
    "sourceRevision",
    "svnConvertedText",
);

# The array of test cases.
my @testCaseHashRefs = (
{
    # New test
    diffName => "SVN: simple",
    inputText => <<'END',
Index: Makefile
===================================================================
--- Makefile	(revision 53052)
+++ Makefile	(working copy)
@@ -1,3 +1,4 @@
+
 MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
 
 all:
END
    # Header keys to check
    svnConvertedText =>  <<'END', # Same as input text
Index: Makefile
===================================================================
--- Makefile	(revision 53052)
+++ Makefile	(working copy)
@@ -1,3 +1,4 @@
+
 MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
 
 all:
END
    copiedFromPath => undef,
    indexPath => "Makefile",
    sourceRevision => "53052",
    # Other values to check
    lastReadLine => undef,
    nextLine => undef,
},
{
    # New test
    diffName => "SVN: leading junk",
    inputText => <<'END',

LEADING JUNK

Index: Makefile
===================================================================
--- Makefile	(revision 53052)
+++ Makefile	(working copy)
@@ -1,3 +1,4 @@
+
 MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
 
 all:
END
    # Header keys to check
    svnConvertedText =>  <<'END', # Same as input text

LEADING JUNK

Index: Makefile
===================================================================
--- Makefile	(revision 53052)
+++ Makefile	(working copy)
@@ -1,3 +1,4 @@
+
 MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
 
 all:
END
    copiedFromPath => undef,
    indexPath => "Makefile",
    sourceRevision => "53052",
    # Other values to check
    lastReadLine => undef,
    nextLine => undef,
},
{
    # New test
    diffName => "SVN: copied file",
    inputText => <<'END',
Index: Makefile_new
===================================================================
--- Makefile_new	(revision 53131)	(from Makefile:53131)
+++ Makefile_new	(working copy)
@@ -0,0 +1,1 @@
+MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
END
    # Header keys to check
    svnConvertedText =>  <<'END', # Same as input text
Index: Makefile_new
===================================================================
--- Makefile_new	(revision 53131)	(from Makefile:53131)
+++ Makefile_new	(working copy)
@@ -0,0 +1,1 @@
+MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
END
    copiedFromPath => "Makefile",
    indexPath => "Makefile_new",
    sourceRevision => "53131",
    # Other values to check
    lastReadLine => undef,
    nextLine => undef,
},
{
    # New test
    diffName => "SVN: two diffs",
    inputText => <<'END',
Index: Makefile
===================================================================
--- Makefile	(revision 53131)
+++ Makefile	(working copy)
@@ -1,1 +0,0 @@
-MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
Index: Makefile_new
===================================================================
--- Makefile_new	(revision 53131)	(from Makefile:53131)
END
    # Header keys to check
    svnConvertedText =>  <<'END',
Index: Makefile
===================================================================
--- Makefile	(revision 53131)
+++ Makefile	(working copy)
@@ -1,1 +0,0 @@
-MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
END
    copiedFromPath => undef,
    indexPath => "Makefile",
    sourceRevision => "53131",
    # Other values to check
    lastReadLine => "Index: Makefile_new\n",
    nextLine => "===================================================================\n",
},
{
    # New test
    diffName => "SVN: SVN diff followed by Git diff", # Should not recognize Git start
    inputText => <<'END',
Index: Makefile
===================================================================
--- Makefile	(revision 53131)
+++ Makefile	(working copy)
@@ -1,1 +0,0 @@
-MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
diff --git a/Makefile b/Makefile
index f5d5e74..3b6aa92 100644
--- a/Makefile
+++ b/Makefile
@@ -1,1 1,1 @@ public:
END
    # Header keys to check
    svnConvertedText =>  <<'END', # Same as input text
Index: Makefile
===================================================================
--- Makefile	(revision 53131)
+++ Makefile	(working copy)
@@ -1,1 +0,0 @@
-MODULES = JavaScriptCore JavaScriptGlue WebCore WebKit WebKitTools
diff --git a/Makefile b/Makefile
index f5d5e74..3b6aa92 100644
--- a/Makefile
+++ b/Makefile
@@ -1,1 1,1 @@ public:
END
    copiedFromPath => undef,
    indexPath => "Makefile",
    sourceRevision => "53131",
    # Other values to check
    lastReadLine => undef,
    nextLine => undef,
},
{
    # New test
    diffName => "Git: simple",
    inputText => <<'END',
diff --git a/Makefile b/Makefile
index f5d5e74..3b6aa92 100644
--- a/Makefile
+++ b/Makefile
@@ -1,1 1,1 @@ public:
END
    # Header keys to check
    svnConvertedText =>  <<'END',
Index: Makefile
===================================================================
--- Makefile
+++ Makefile
@@ -1,1 1,1 @@ public:
END
    copiedFromPath => undef,
    indexPath => "Makefile",
    sourceRevision => undef,
    # Other values to check
    lastReadLine => undef,
    nextLine => undef,
},
{
    # New test
    diffName => "Git: Git diff followed by SVN diff", # Should not recognize SVN start
    inputText => <<'END',
diff --git a/Makefile b/Makefile
index f5d5e74..3b6aa92 100644
--- a/Makefile
+++ b/Makefile
@@ -1,1 1,1 @@ public:
Index: Makefile_new
===================================================================
--- Makefile_new	(revision 53131)	(from Makefile:53131)
END
    # Header keys to check
    svnConvertedText =>  <<'END',
Index: Makefile
===================================================================
--- Makefile
+++ Makefile
@@ -1,1 1,1 @@ public:
Index: Makefile_new
===================================================================
--- Makefile_new	(revision 53131)	(from Makefile:53131)
END
    copiedFromPath => undef,
    indexPath => "Makefile",
    sourceRevision => undef,
    # Other values to check
    lastReadLine => undef,
    nextLine => undef,
},
);

# Return the arguments for each assertion per test case.
#
# In particular, the number of assertions per test case is the length
# of the return value of this subroutine on a sample input.
#
# Returns @assertionArgsArrayRefs:
#   $assertionArgsArrayRef: A reference to an array of parameters to pass
#                           to each call to is(). The parameters are--
#                             $got: The value obtained
#                             $expected: The expected value
#                             $testName: The name of the test
sub testParseDiffAssertionArgs($)
{
    my ($testCaseHashRef) = @_;

    my $fileHandle;
    open($fileHandle, "<", \$testCaseHashRef->{inputText});

    my $line = <$fileHandle>;

    my ($diffHashRef, $lastReadLine) = VCSUtils::parseDiff($fileHandle, $line);

    my $testNameStart = "parseDiff(): [$testCaseHashRef->{diffName}] ";

    my @assertionArgsArrayRefs; # Return value
    my @assertionArgs;
    foreach my $diffHashRefKey (@diffHashRefKeys) {
        my $testName = "${testNameStart}key=\"$diffHashRefKey\"";
        @assertionArgs = ($diffHashRef->{$diffHashRefKey}, $testCaseHashRef->{$diffHashRefKey}, $testName);
        push(@assertionArgsArrayRefs, \@assertionArgs);
    }

    @assertionArgs = ($lastReadLine, $testCaseHashRef->{lastReadLine}, "${testNameStart}lastReadLine");
    push(@assertionArgsArrayRefs, \@assertionArgs);

    my $nextLine = <$fileHandle>;
    @assertionArgs = ($nextLine, $testCaseHashRef->{nextLine}, "${testNameStart}nextLine");
    push(@assertionArgsArrayRefs, \@assertionArgs);

    return @assertionArgsArrayRefs;
}

# Test parseDiff() for the given test case.
sub testParseDiff($)
{
    my ($testCaseHashRef) = @_;

    my @assertionArgsArrayRefs = testParseDiffAssertionArgs($testCaseHashRef);

    foreach my $arrayRef (@assertionArgsArrayRefs) {
        # The parameters are -- is($got, $expected, $testName).
        is($arrayRef->[0], $arrayRef->[1], $arrayRef->[2]);
    }
}

# Count the number of assertions per test case, using a sample test case.
my $assertionCount = testParseDiffAssertionArgs($testCaseHashRefs[0]);

plan(tests => @testCaseHashRefs * $assertionCount); # Total number of tests

foreach my $testCaseHashRef (@testCaseHashRefs) {
    testParseDiff($testCaseHashRef);
}
