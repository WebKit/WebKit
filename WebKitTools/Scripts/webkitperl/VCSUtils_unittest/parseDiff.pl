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
    expectedReturn => [
{
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
},
undef],
    expectedNextLine => undef,
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
    expectedReturn => [
{
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
},
undef],
    expectedNextLine => undef,
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
    expectedReturn => [
{
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
},
undef],
    expectedNextLine => undef,
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
    expectedReturn => [
{
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
},
"Index: Makefile_new\n"],
    expectedNextLine => "===================================================================\n",
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
    expectedReturn => [
{
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
},
undef],
    expectedNextLine => undef,
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
    expectedReturn => [
{
    svnConvertedText =>  <<'END',
Index: Makefile
index f5d5e74..3b6aa92 100644
--- Makefile
+++ Makefile
@@ -1,1 1,1 @@ public:
END
    copiedFromPath => undef,
    indexPath => "Makefile",
    sourceRevision => undef,
},
undef],
    expectedNextLine => undef,
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
    expectedReturn => [
{
    svnConvertedText =>  <<'END',
Index: Makefile
index f5d5e74..3b6aa92 100644
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
},
undef],
    expectedNextLine => undef,
},
);

my $testCasesCount = @testCaseHashRefs;
plan(tests => 2 * $testCasesCount); # Total number of assertions.

foreach my $testCase (@testCaseHashRefs) {
    my $testNameStart = "parseDiff(): $testCase->{diffName}: comparing";

    my $fileHandle;
    open($fileHandle, "<", \$testCase->{inputText});
    my $line = <$fileHandle>;

    my @got = VCSUtils::parseDiff($fileHandle, $line);
    my $expectedReturn = $testCase->{expectedReturn};

    is_deeply(\@got, $expectedReturn, "$testNameStart return value.");

    my $gotNextLine = <$fileHandle>;
    is($gotNextLine, $testCase->{expectedNextLine},  "$testNameStart next read line.");
}
