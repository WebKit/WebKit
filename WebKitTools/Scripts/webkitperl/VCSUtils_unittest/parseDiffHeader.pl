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

my @diffHeaderHashRefKeys = ( # The $diffHeaderHashRef keys to check.
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
Index: WebKitTools/Scripts/VCSUtils.pm
===================================================================
--- WebKitTools/Scripts/VCSUtils.pm	(revision 53004)
+++ WebKitTools/Scripts/VCSUtils.pm	(working copy)
@@ -32,6 +32,7 @@ use strict;
 use warnings;
END
    # Header keys to check
    svnConvertedText => <<'END',
Index: WebKitTools/Scripts/VCSUtils.pm
===================================================================
--- WebKitTools/Scripts/VCSUtils.pm	(revision 53004)
+++ WebKitTools/Scripts/VCSUtils.pm	(working copy)
END
    copiedFromPath => undef,
    indexPath => "WebKitTools/Scripts/VCSUtils.pm",
    sourceRevision => "53004",
    # Other values to check
    lastReadLine => "@@ -32,6 +32,7 @@ use strict;\n",
    nextLine => " use warnings;\n",
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
    # Header keys to check
    svnConvertedText => <<'END',
Index: WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl
===================================================================
--- WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl	(revision 0)
+++ WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl	(revision 0)
END
    copiedFromPath => undef,
    indexPath => "WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl",
    sourceRevision => undef,
    # Other values to check
    lastReadLine => "@@ -0,0 +1,262 @@\n",
    nextLine => "+#!/usr/bin/perl -w\n",
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
    # Header keys to check
    svnConvertedText => <<'END',
Index: index_path.py
===================================================================
--- index_path.py	(revision 53048)	(from copied_from_path.py:53048)
+++ index_path.py	(working copy)
END
    copiedFromPath => "copied_from_path.py",
    indexPath => "index_path.py",
    sourceRevision => 53048,
    # Other values to check
    lastReadLine => "@@ -0,0 +1,7 @@\n",
    nextLine => "+# Python file...\n",
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
    # Header keys to check
    svnConvertedText => <<END, # No single quotes to allow interpolation of "\r"
Index: index_path.py\r
===================================================================\r
--- index_path.py	(revision 53048)	(from copied_from_path.py:53048)\r
+++ index_path.py	(working copy)\r
END
    copiedFromPath => "copied_from_path.py",
    indexPath => "index_path.py",
    sourceRevision => 53048,
    # Other values to check
    lastReadLine => "@@ -0,0 +1,7 @@\r\n",
    nextLine => "+# Python file...\r\n",
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
    # Header keys to check
    svnConvertedText => <<'END',
Index: index_path.py
===================================================================
--- index_path.py	(revision 53048)	(from copied_from_path.py:53048)
+++ index_path.py	(working copy)
END
    copiedFromPath => "copied_from_path.py",
    indexPath => "index_path.py",
    sourceRevision => 53048,
    # Other values to check
    lastReadLine => "@@ -0,0 +1,7 @@\n",
    nextLine => "+# Python file...\n",
},
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
    # Header keys to check
    svnConvertedText => <<'END',
Index: WebCore/rendering/style/StyleFlexibleBoxData.h
index f5d5e74..3b6aa92 100644
--- WebCore/rendering/style/StyleFlexibleBoxData.h
+++ WebCore/rendering/style/StyleFlexibleBoxData.h
END
    copiedFromPath => undef,
    indexPath => "WebCore/rendering/style/StyleFlexibleBoxData.h",
    sourceRevision => undef,
    # Other values to check
    lastReadLine => "@@ -47,7 +47,6 @@ public:\n",
    nextLine => undef,
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
    # Header keys to check
    svnConvertedText => <<'END',
Index: LayoutTests/http/tests/security/listener/xss-inactive-closure.html
new file mode 100644
index 0000000..3c9f114
--- LayoutTests/http/tests/security/listener/xss-inactive-closure.html
+++ LayoutTests/http/tests/security/listener/xss-inactive-closure.html
END
    copiedFromPath => undef,
    indexPath => "LayoutTests/http/tests/security/listener/xss-inactive-closure.html",
    sourceRevision => undef,
    # Other values to check
    lastReadLine => "@@ -0,0 +1,34 @@\n",
    nextLine => "+<html>\n",
},
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
    # Header keys to check
    svnConvertedText => <<'END',
Index: test_file.swf
===================================================================
Cannot display: file marked as a binary type.
END
    copiedFromPath => undef,
    indexPath => "test_file.swf",
    sourceRevision => undef,
    # Other values to check
    lastReadLine => "svn:mime-type = application/octet-stream\n",
    nextLine => "\n",
},
{
    # New test
    diffName => "Git: binary addition",
    inputText => <<'END',
diff --git a/foo.gif b/foo.gif
new file mode 100644
index 0000000000000000000000000000000000000000..64a9532e7794fcd791f6f12157406d9060151690
GIT binary patch
literal 512
zcmZ?wbhEHbRAx|MU|?iW{Kxc~?KofD;ckY;H+&5HnHl!!GQMD7h+sU{_)e9f^V3c?
zhJP##HdZC#4K}7F68@!1jfWQg2daCm-gs#3|JREDT>c+pG4L<_2;w##WMO#ysPPap
zLqpAf1OE938xAsSp4!5f-o><?VKe(#0jEcwfHGF4%M1^kRs14oVBp2ZEL{E1N<-zJ
zsfLmOtKta;2_;2c#^S1-8cf<nb!QnGl>c!Xe6RXvrEtAWBvSDTgTO1j3vA31Puw!A
zs(87q)j_mVDTqBo-P+03-P5mHCEnJ+x}YdCuS7#bCCyePUe(ynK+|4b-3qK)T?Z&)
zYG+`tl4h?GZv_$t82}X4*DTE|$;{DEiPyF@)U-1+FaX++T9H{&%cag`W1|zVP@`%b
zqiSkp6{BTpWTkCr!=<C6Q=?#~R8^JfrliAF6Q^gV9Iup8RqCXqqhqC`qsyhk<-nlB
z00f{QZvfK&|Nm#oZ0TQl`Yr$BIa6A@16O26ud7H<QM=xl`toLKnz-3h@9c9q&wm|X
z{89I|WPyD!*M?gv?q`;L=2YFeXrJQNti4?}s!zFo=5CzeBxC69xA<zrjP<wUcCRh4
ptUl-ZG<%a~#LwkIWv&q!KSCH7tQ8cJDiw+|GV?MN)RjY50RTb-xvT&H

literal 0
HcmV?d00001

END
    # Header keys to check
    svnConvertedText => <<'END',
Index: foo.gif
new file mode 100644
index 0000000000000000000000000000000000000000..64a9532e7794fcd791f6f12157406d9060151690
GIT binary patch
END
    copiedFromPath => undef,
    indexPath => "foo.gif",
    sourceRevision => undef,
    # Other values to check
    lastReadLine => "literal 512\n",
    nextLine => "zcmZ?wbhEHbRAx|MU|?iW{Kxc~?KofD;ckY;H+&5HnHl!!GQMD7h+sU{_)e9f^V3c?\n",
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
    # Header keys to check
    svnConvertedText => <<'END',
Index: foo.gif
deleted file mode 100644
index 323fae0..0000000
GIT binary patch
END
    copiedFromPath => undef,
    indexPath => "foo.gif",
    sourceRevision => undef,
    # Other values to check
    lastReadLine => "literal 0\n",
    nextLine => "HcmV?d00001\n",
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
sub testParseDiffHeaderAssertionArgs($)
{
    my ($testCaseHashRef) = @_;

    my $fileHandle;
    open($fileHandle, "<", \$testCaseHashRef->{inputText});

    my $line = <$fileHandle>;

    my ($headerHashRef, $lastReadLine) = VCSUtils::parseDiffHeader($fileHandle, $line);

    my $testNameStart = "parseDiffHeader(): [$testCaseHashRef->{diffName}] ";

    my @assertionArgsArrayRefs; # Return value
    my $assertionArgsRef;

    foreach my $diffHeaderHashRefKey (@diffHeaderHashRefKeys) {
        my $testName = "${testNameStart}key=\"$diffHeaderHashRefKey\"";
        $assertionArgsRef = [$headerHashRef->{$diffHeaderHashRefKey}, $testCaseHashRef->{$diffHeaderHashRefKey}, $testName];
        push(@assertionArgsArrayRefs, $assertionArgsRef);
    }

    $assertionArgsRef = [$lastReadLine, $testCaseHashRef->{lastReadLine}, "${testNameStart}lastReadLine"];
    push(@assertionArgsArrayRefs, $assertionArgsRef);

    my $nextLine = <$fileHandle>;
    $assertionArgsRef = [$nextLine, $testCaseHashRef->{nextLine}, "${testNameStart}nextLine"];
    push(@assertionArgsArrayRefs, $assertionArgsRef);

    return @assertionArgsArrayRefs;
}

# Test parseDiffHeader() for the given test case.
sub testParseDiffHeader($)
{
    my ($testCaseHashRef) = @_;

    my @assertionArgsArrayRefs = testParseDiffHeaderAssertionArgs($testCaseHashRef);

    foreach my $arrayRef (@assertionArgsArrayRefs) {
        # The parameters are -- is($got, $expected, $testName).
        is($arrayRef->[0], $arrayRef->[1], $arrayRef->[2]);
    }
}

# Count the number of assertions per test case to calculate the total number
# of Test::More tests. We could have used any test case for the count.
my $assertionCount = testParseDiffHeaderAssertionArgs($testCaseHashRefs[0]);

plan(tests => @testCaseHashRefs * $assertionCount); # Total number of tests

foreach my $testCaseHashRef (@testCaseHashRefs) {
    testParseDiffHeader($testCaseHashRef);
}
