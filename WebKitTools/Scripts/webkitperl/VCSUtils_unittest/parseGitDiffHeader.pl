#!/usr/bin/perl -w
#
# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Unit tests of parseGitDiffHeader().

use strict;
use warnings;

use Test::More;
use VCSUtils;

# The array of test cases.
my @testCaseHashRefs = (
{   # New test
    diffName => "Modified file",
    inputText => <<'END',
diff --git a/foo.h b/foo.h
index f5d5e74..3b6aa92 100644
--- a/foo.h
+++ b/foo.h
@@ -1 +1 @@
-file contents
+new file contents
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo.h
index f5d5e74..3b6aa92 100644
--- foo.h
+++ foo.h
END
    indexPath => "foo.h",
},
"@@ -1 +1 @@\n"],
    expectedNextLine => "-file contents\n",
},
{   # New test
    diffName => "new file",
    inputText => <<'END',
diff --git a/foo.h b/foo.h
new file mode 100644
index 0000000..3c9f114
--- /dev/null
+++ b/foo.h
@@ -0,0 +1,34 @@
+<html>
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo.h
new file mode 100644
index 0000000..3c9f114
--- foo.h
+++ foo.h
END
    indexPath => "foo.h",
    isNew => 1,
},
"@@ -0,0 +1,34 @@\n"],
    expectedNextLine => "+<html>\n",
},
{   # New test
    diffName => "file deletion",
    inputText => <<'END',
diff --git a/foo b/foo
deleted file mode 100644
index 1e50d1d..0000000
--- a/foo
+++ /dev/null
@@ -1,1 +0,0 @@
-line1
diff --git a/configure.ac b/configure.ac
index d45dd40..3494526 100644
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo
deleted file mode 100644
index 1e50d1d..0000000
--- foo
+++ foo
END
    indexPath => "foo",
    isDeletion => 1,
},
"@@ -1,1 +0,0 @@\n"],
    expectedNextLine => "-line1\n",
},
{   # New test
    diffName => "using --no-prefix",
    inputText => <<'END',
diff --git foo.h foo.h
index c925780..9e65c43 100644
--- foo.h
+++ foo.h
@@ -1,3 +1,17 @@
+contents
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo.h
index c925780..9e65c43 100644
--- foo.h
+++ foo.h
END
    indexPath => "foo.h",
},
"@@ -1,3 +1,17 @@\n"],
    expectedNextLine => "+contents\n",
},
####
#    Copy operations
##
{   # New test
    diffName => "copy (with similarity index 100%)",
    inputText => <<'END',
diff --git a/foo b/foo_new
similarity index 100%
copy from foo
copy to foo_new
diff --git a/bar b/bar
index d45dd40..3494526 100644
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo_new
similarity index 100%
copy from foo
copy to foo_new
END
    copiedFromPath => "foo",
    indexPath => "foo_new",
},
"diff --git a/bar b/bar\n"],
    expectedNextLine => "index d45dd40..3494526 100644\n",
},
{   # New test
    diffName => "copy (with similarity index < 100%)",
    inputText => <<'END',
diff --git a/foo b/foo_new
similarity index 99%
copy from foo
copy to foo_new
diff --git a/bar b/bar
index d45dd40..3494526 100644
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo_new
similarity index 99%
copy from foo
copy to foo_new
END
    copiedFromPath => "foo",
    indexPath => "foo_new",
    isCopyWithChanges => 1,
},
"diff --git a/bar b/bar\n"],
    expectedNextLine => "index d45dd40..3494526 100644\n",
},
{   # New test
    diffName => "rename (with similarity index 100%)",
    inputText => <<'END',
diff --git a/foo b/foo_new
similarity index 100%
rename from foo
rename to foo_new
diff --git a/bar b/bar
index d45dd40..3494526 100644
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo_new
similarity index 100%
rename from foo
rename to foo_new
END
    copiedFromPath => "foo",
    indexPath => "foo_new",
    shouldDeleteSource => 1,
},
"diff --git a/bar b/bar\n"],
    expectedNextLine => "index d45dd40..3494526 100644\n",
},
{   # New test
    diffName => "rename (with similarity index < 100%)",
    inputText => <<'END',
diff --git a/foo b/foo_new
similarity index 99%
rename from foo
rename to foo_new
index 1e50d1d..1459d21 100644
--- a/foo
+++ b/foo_new
@@ -15,3 +15,4 @@ release r deployment dep deploy:
 line1
 line2
 line3
+line4
diff --git a/bar b/bar
index d45dd40..3494526 100644
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo_new
similarity index 99%
rename from foo
rename to foo_new
index 1e50d1d..1459d21 100644
--- foo_new
+++ foo_new
END
    copiedFromPath => "foo",
    indexPath => "foo_new",
    isCopyWithChanges => 1,
    shouldDeleteSource => 1,
},
"@@ -15,3 +15,4 @@ release r deployment dep deploy:\n"],
    expectedNextLine => " line1\n",
},
{   # New test
    diffName => "rename (with executable bit change)",
    inputText => <<'END',
diff --git a/foo b/foo_new
old mode 100644
new mode 100755
similarity index 100%
rename from foo
rename to foo_new
diff --git a/bar b/bar
index d45dd40..3494526 100644
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo_new
old mode 100644
new mode 100755
similarity index 100%
rename from foo
rename to foo_new
END
    copiedFromPath => "foo",
    executableBitDelta => 1,
    indexPath => "foo_new",
    isCopyWithChanges => 1,
    shouldDeleteSource => 1,
},
"diff --git a/bar b/bar\n"],
    expectedNextLine => "index d45dd40..3494526 100644\n",
},
####
#    Binary file test cases
##
{
    # New test case
    diffName => "New binary file",
    inputText => <<'END',
diff --git a/foo.gif b/foo.gif
new file mode 100644
index 0000000000000000000000000000000000000000..64a9532e7794fcd791f6f12157406d9060151690
GIT binary patch
literal 7
OcmYex&reDa;sO8*F9L)B

literal 0
HcmV?d00001

END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo.gif
new file mode 100644
index 0000000000000000000000000000000000000000..64a9532e7794fcd791f6f12157406d9060151690
GIT binary patch
END
    indexPath => "foo.gif",
    isBinary => 1,
    isNew => 1,
},
"literal 7\n"],
    expectedNextLine => "OcmYex&reDa;sO8*F9L)B\n",
},
{
    # New test case
    diffName => "Deleted binary file",
    inputText => <<'END',
diff --git a/foo.gif b/foo.gif
deleted file mode 100644
index 323fae0..0000000
GIT binary patch
literal 0
HcmV?d00001

literal 7
OcmYex&reDa;sO8*F9L)B

END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo.gif
deleted file mode 100644
index 323fae0..0000000
GIT binary patch
END
    indexPath => "foo.gif",
    isBinary => 1,
    isDeletion => 1,
},
"literal 0\n"],
    expectedNextLine => "HcmV?d00001\n",
},
####
#    Executable bit test cases
##
{
    # New test case
    diffName => "Modified executable file",
    inputText => <<'END',
diff --git a/foo b/foo
index d03e242..435ad3a 100755
--- a/foo
+++ b/foo
@@ -1 +1 @@
-file contents
+new file contents

END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo
index d03e242..435ad3a 100755
--- foo
+++ foo
END
    indexPath => "foo",
},
"@@ -1 +1 @@\n"],
    expectedNextLine => "-file contents\n",
},
{
    # New test case
    diffName => "Making file executable (last diff)",
    inputText => <<'END',
diff --git a/foo.exe b/foo.exe
old mode 100644
new mode 100755
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo.exe
old mode 100644
new mode 100755
END
    executableBitDelta => 1,
    indexPath => "foo.exe",
},
undef],
    expectedNextLine => undef,
},
{
    # New test case
    diffName => "Making file executable (not last diff)",
    inputText => <<'END',
diff --git a/foo.exe b/foo.exe
old mode 100644
new mode 100755
diff --git a/another_file.txt b/another_file.txt
index d03e242..435ad3a 100755
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo.exe
old mode 100644
new mode 100755
END
    executableBitDelta => 1,
    indexPath => "foo.exe",
},
"diff --git a/another_file.txt b/another_file.txt\n"],
    expectedNextLine => "index d03e242..435ad3a 100755\n",
},
{
    # New test case
    diffName => "New executable file",
    inputText => <<'END',
diff --git a/foo b/foo
new file mode 100755
index 0000000..d03e242
--- /dev/null
+++ b/foo
@@ -0,0 +1 @@
+file contents

END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo
new file mode 100755
index 0000000..d03e242
--- foo
+++ foo
END
    executableBitDelta => 1,
    indexPath => "foo",
    isNew => 1,
},
"@@ -0,0 +1 @@\n"],
    expectedNextLine => "+file contents\n",
},
{
    # New test case
    diffName => "Deleted executable file",
    inputText => <<'END',
diff --git a/foo b/foo
deleted file mode 100755
index d03e242..0000000
--- a/foo
+++ /dev/null
@@ -1 +0,0 @@
-file contents

END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo
deleted file mode 100755
index d03e242..0000000
--- foo
+++ foo
END
    executableBitDelta => -1,
    indexPath => "foo",
    isDeletion => 1,
},
"@@ -1 +0,0 @@\n"],
    expectedNextLine => "-file contents\n",
},
);

my $testCasesCount = @testCaseHashRefs;
plan(tests => 2 * $testCasesCount); # Total number of assertions.

foreach my $testCase (@testCaseHashRefs) {
    my $testNameStart = "parseGitDiffHeader(): $testCase->{diffName}: comparing";

    my $fileHandle;
    open($fileHandle, "<", \$testCase->{inputText});
    my $line = <$fileHandle>;

    my @got = VCSUtils::parseGitDiffHeader($fileHandle, $line);
    my $expectedReturn = $testCase->{expectedReturn};

    is_deeply(\@got, $expectedReturn, "$testNameStart return value.");

    my $gotNextLine = <$fileHandle>;
    is($gotNextLine, $testCase->{expectedNextLine},  "$testNameStart next read line.");
}
