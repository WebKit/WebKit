#!/usr/bin/env perl
#
# Copyright (C) 2015 Apple Inc.  All rights reserved.
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

# Unit tests for parseUnifiedDiffHeader()

use strict;
use warnings;

use Test::More;
use VCSUtils;

my @testCaseHashRefs = (
{
    diffName => "Modified file",
    inputText => <<'END',
--- Foo/bar.h
+++ Foo/bar.h
@@ -304,6 +304,6 @@ void someKindOfFunction()
leading
text
context
-file contents
+new file contents
trailing
textcontext
END
    expectedReturn => [
    {
        svnConvertedText => <<'END',
Index: Foo/bar.h
index 0000000..0000000
--- Foo/bar.h
+++ Foo/bar.h
END
        indexPath => 'Foo/bar.h',
    },
    "@@ -304,6 +304,6 @@ void someKindOfFunction()\n"],
    expectedNextLine => "leading\n",
},
{
    diffName => "Modified file with spaces in name",
    inputText => <<'END',
--- Foo/bar baz.h
+++ Foo/bar baz.h
@@ -304,6 +304,6 @@ void someKindOfFunction()
leading
text
context
-file contents
+new file contents
trailing
textcontext
END
    expectedReturn => [
    {
        svnConvertedText => <<'END',
Index: Foo/bar baz.h
index 0000000..0000000
--- Foo/bar baz.h
+++ Foo/bar baz.h
END
        indexPath => 'Foo/bar baz.h',
    },
    "@@ -304,6 +304,6 @@ void someKindOfFunction()\n"],
    expectedNextLine => "leading\n",
},
{
    diffName => "Modified file with git-style indices",
    inputText => <<'END',
--- a/Foo/bar.h
+++ b/Foo/bar.h
@@ -304,6 +304,6 @@ void someKindOfFunction()
leading
text
context
-file contents
+new file contents
trailing
textcontext
END
    expectedReturn => [
    {
        svnConvertedText => <<'END',
Index: Foo/bar.h
index 0000000..0000000
--- Foo/bar.h
+++ Foo/bar.h
END
        indexPath => 'Foo/bar.h',
    },
    "@@ -304,6 +304,6 @@ void someKindOfFunction()\n"],
    expectedNextLine => "leading\n",
},
{
    diffName => "Added file",
    inputText => <<'END',
--- /dev/null
+++ Foo/bar.h
@@ -0,0 +1,6 @@
+leading
+text
+context
+new file contents
+trailing
+textcontext
END
    expectedReturn => [
    {
        svnConvertedText => <<'END',
Index: Foo/bar.h
new file mode 100644
index 0000000..0000000
--- /dev/null
+++ Foo/bar.h
END
        indexPath => 'Foo/bar.h',
        isNew => 1,
    },
    "@@ -0,0 +1,6 @@\n"],
    expectedNextLine => "+leading\n",
},
{
    diffName => "Removed file",
    inputText => <<'END',
--- Foo/bar.h
+++ /dev/null
@@ -0,0 +1,6 @@
-leading
-text
-context
-new file contents
-trailing
-textcontext
END
    expectedReturn => [
    {
        svnConvertedText => <<'END',
Index: Foo/bar.h
index 0000000..0000000
--- Foo/bar.h
+++ /dev/null
END
        indexPath => 'Foo/bar.h',
        isDeletion => 1,
    },
    "@@ -0,0 +1,6 @@\n"],
    expectedNextLine => "-leading\n",
},
);


my $testCasesCount = @testCaseHashRefs;
plan(tests => 2 * $testCasesCount); # Total number of assertions.

foreach my $testCase (@testCaseHashRefs) {
    my $testNameStart = "parseUnifiedDiffHeader(): $testCase->{diffName}: comparing";

    my $fileHandle;
    open($fileHandle, "<", \$testCase->{inputText});
    my $line = <$fileHandle>;

    my @got = VCSUtils::parseUnifiedDiffHeader($fileHandle, $line);
    my $expectedReturn = $testCase->{expectedReturn};

    is_deeply(\@got, $expectedReturn, "$testNameStart return value.");

    my $gotNextLine = <$fileHandle>;
    is($gotNextLine, $testCase->{expectedNextLine},  "$testNameStart next read line.");
}
