#!/usr/bin/perl -w
#
# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Apple Computer, Inc. ("Apple") nor the names of
# its contributors may be used to endorse or promote products derived
# from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Unit tests of parseDiffHeader().

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
Index: WebKitTools/Scripts/VCSUtils.pm
===================================================================
--- WebKitTools/Scripts/VCSUtils.pm	(revision 53004)
+++ WebKitTools/Scripts/VCSUtils.pm	(working copy)
@@ -32,6 +32,7 @@ use strict;
 use warnings;
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: WebKitTools/Scripts/VCSUtils.pm
===================================================================
--- WebKitTools/Scripts/VCSUtils.pm	(revision 53004)
+++ WebKitTools/Scripts/VCSUtils.pm	(working copy)
END
    copiedFromPath => undef,
    indexPath => "WebKitTools/Scripts/VCSUtils.pm",
    scmFormat => "svn",
    sourceRevision => "53004",
},
"@@ -32,6 +32,7 @@ use strict;\n"],
    expectedNextLine => " use warnings;\n",
},
{
    # New test
    diffName => "SVN: new file",
    inputText => <<'END',
Index: WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl
===================================================================
--- WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl	(revision 0)
+++ WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl	(revision 0)
@@ -0,0 +1,262 @@
+#!/usr/bin/perl -w
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl
===================================================================
--- WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl	(revision 0)
+++ WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl	(revision 0)
END
    copiedFromPath => undef,
    indexPath => "WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl",
    scmFormat => "svn",
    sourceRevision => undef,
},
"@@ -0,0 +1,262 @@\n"],
    expectedNextLine => "+#!/usr/bin/perl -w\n",
},
{
    # New test
    diffName => "SVN: copy",
    inputText => <<'END',
Index: index_path.py
===================================================================
--- index_path.py	(revision 53048)	(from copied_from_path.py:53048)
+++ index_path.py	(working copy)
@@ -0,0 +1,7 @@
+# Python file...
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: index_path.py
===================================================================
--- index_path.py	(revision 53048)	(from copied_from_path.py:53048)
+++ index_path.py	(working copy)
END
    copiedFromPath => "copied_from_path.py",
    indexPath => "index_path.py",
    scmFormat => "svn",
    sourceRevision => 53048,
},
"@@ -0,0 +1,7 @@\n"],
    expectedNextLine => "+# Python file...\n",
},
{
    # New test
    diffName => "SVN: \\r\\n lines",
    inputText => <<END, # No single quotes to allow interpolation of "\r"
Index: index_path.py\r
===================================================================\r
--- index_path.py	(revision 53048)	(from copied_from_path.py:53048)\r
+++ index_path.py	(working copy)\r
@@ -0,0 +1,7 @@\r
+# Python file...\r
END
    expectedReturn => [
{
    svnConvertedText => <<END, # No single quotes to allow interpolation of "\r"
Index: index_path.py\r
===================================================================\r
--- index_path.py	(revision 53048)	(from copied_from_path.py:53048)\r
+++ index_path.py	(working copy)\r
END
    copiedFromPath => "copied_from_path.py",
    indexPath => "index_path.py",
    scmFormat => "svn",
    sourceRevision => 53048,
},
"@@ -0,0 +1,7 @@\r\n"],
    expectedNextLine => "+# Python file...\r\n",
},
{
    # New test
    diffName => "SVN: path corrections",
    inputText => <<'END',
Index: index_path.py
===================================================================
--- bad_path	(revision 53048)	(from copied_from_path.py:53048)
+++ bad_path	(working copy)
@@ -0,0 +1,7 @@
+# Python file...
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: index_path.py
===================================================================
--- index_path.py	(revision 53048)	(from copied_from_path.py:53048)
+++ index_path.py	(working copy)
END
    copiedFromPath => "copied_from_path.py",
    indexPath => "index_path.py",
    scmFormat => "svn",
    sourceRevision => 53048,
},
"@@ -0,0 +1,7 @@\n"],
    expectedNextLine => "+# Python file...\n",
},
####
#    Git test cases
##
{
    # New test
    diffName => "Git: simple",
    inputText => <<'END',
diff --git a/WebCore/rendering/style/StyleFlexibleBoxData.h b/WebCore/rendering/style/StyleFlexibleBoxData.h
index f5d5e74..3b6aa92 100644
--- a/WebCore/rendering/style/StyleFlexibleBoxData.h
+++ b/WebCore/rendering/style/StyleFlexibleBoxData.h
@@ -47,7 +47,6 @@ public:
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: WebCore/rendering/style/StyleFlexibleBoxData.h
index f5d5e74..3b6aa92 100644
--- WebCore/rendering/style/StyleFlexibleBoxData.h
+++ WebCore/rendering/style/StyleFlexibleBoxData.h
END
    copiedFromPath => undef,
    indexPath => "WebCore/rendering/style/StyleFlexibleBoxData.h",
    scmFormat => "git",
    sourceRevision => undef,
},
"@@ -47,7 +47,6 @@ public:\n"],
    expectedNextLine => undef,
},
{
    # New test
    diffName => "Git: new file",
    inputText => <<'END',
diff --git a/LayoutTests/http/tests/security/listener/xss-inactive-closure.html b/LayoutTests/http/tests/security/listener/xss-inactive-closure.html
new file mode 100644
index 0000000..3c9f114
--- /dev/null
+++ b/LayoutTests/http/tests/security/listener/xss-inactive-closure.html
@@ -0,0 +1,34 @@
+<html>
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: LayoutTests/http/tests/security/listener/xss-inactive-closure.html
new file mode 100644
index 0000000..3c9f114
--- LayoutTests/http/tests/security/listener/xss-inactive-closure.html
+++ LayoutTests/http/tests/security/listener/xss-inactive-closure.html
END
    copiedFromPath => undef,
    indexPath => "LayoutTests/http/tests/security/listener/xss-inactive-closure.html",
    scmFormat => "git",
    sourceRevision => undef,
},
"@@ -0,0 +1,34 @@\n"],
    expectedNextLine => "+<html>\n",
},
####
#    Binary test cases
##
{
    # New test
    diffName => "SVN: binary file",
    inputText => <<'END',
Index: test_file.swf
===================================================================
Cannot display: file marked as a binary type.
svn:mime-type = application/octet-stream

Property changes on: test_file.swf
___________________________________________________________________
Name: svn:mime-type
   + application/octet-stream


Q1dTBx0AAAB42itg4GlgYJjGwMDDyODMxMDw34GBgQEAJPQDJA==
END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: test_file.swf
===================================================================
Cannot display: file marked as a binary type.
END
    copiedFromPath => undef,
    indexPath => "test_file.swf",
    scmFormat => "svn",
    sourceRevision => undef,
},
"svn:mime-type = application/octet-stream\n"],
    expectedNextLine => "\n",
},
{
    # New test
    diffName => "Git: binary addition",
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
    copiedFromPath => undef,
    indexPath => "foo.gif",
    scmFormat => "git",
    sourceRevision => undef,
},
"literal 7\n"],
    expectedNextLine => "OcmYex&reDa;sO8*F9L)B\n",
},
{
    # New test
    diffName => "Git: binary deletion",
    inputText => <<'END',
diff --git a/foo.gif b/foo.gif
deleted file mode 100644
index 323fae0..0000000
GIT binary patch
literal 0
HcmV?d00001

literal 7
OcmYex&reD$;sO8*F9L)B

END
    expectedReturn => [
{
    svnConvertedText => <<'END',
Index: foo.gif
deleted file mode 100644
index 323fae0..0000000
GIT binary patch
END
    copiedFromPath => undef,
    indexPath => "foo.gif",
    scmFormat => "git",
    sourceRevision => undef,
},
"literal 0\n"],
    expectedNextLine => "HcmV?d00001\n",
},
);

my $testCasesCount = @testCaseHashRefs;
plan(tests => 2 * $testCasesCount); # Total number of assertions.

foreach my $testCase (@testCaseHashRefs) {
    my $testNameStart = "parseDiffHeader(): $testCase->{diffName}: comparing";

    my $fileHandle;
    open($fileHandle, "<", \$testCase->{inputText});
    my $line = <$fileHandle>;

    my @got = VCSUtils::parseDiffHeader($fileHandle, $line);
    my $expectedReturn = $testCase->{expectedReturn};

    is_deeply(\@got, $expectedReturn, "$testNameStart return value.");

    my $gotNextLine = <$fileHandle>;
    is($gotNextLine, $testCase->{expectedNextLine},  "$testNameStart next read line.");
}
