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

my @headerKeys = ( # The $headerHashRef keys to check.
    "copiedFromPath",
    "copiedFromVersion",
    "indexPath",
    "svnConvertedText",
    "version",
);

# Test parseDiffHeader() for the given $diffTestHashRef.
sub doDiffTest($)
{
    my ($diffTestHashRef) = @_;

    my $fileHandle;
    open($fileHandle, "<", \$diffTestHashRef->{inputText});

    my $line = <$fileHandle>;

    my ($headerHashRef, $foundHeaderEnding, $lastReadLine) = VCSUtils::parseDiffHeader($fileHandle, $line);

    my $titleHeader = "parseDiffHeader(): [$diffTestHashRef->{diffName}] ";
    my $title;

    foreach my $headerKey (@headerKeys) {
        my $title = "${titleHeader}key=\"$headerKey\"";
        is($headerHashRef->{$headerKey}, $diffTestHashRef->{$headerKey}, $title);
    }

    is($foundHeaderEnding, $diffTestHashRef->{foundHeaderEnding}, "${titleHeader}foundHeaderEnding");
    is($lastReadLine, $diffTestHashRef->{lastReadLine}, "${titleHeader}lastReadLine");

    my $nextLine = <$fileHandle>;
    is($nextLine, $diffTestHashRef->{nextLine}, "${titleHeader}nextLine");
}

my @diffTests = ({
    # New test: missing closing header line
    diffName => "SVN: incomplete header",
    inputText => <<'END',
Index: WebKitTools/Scripts/VCSUtils.pm
END
    # Header keys to check
    svnConvertedText => <<'END',
Index: WebKitTools/Scripts/VCSUtils.pm
END
    copiedFromPath => undef,
    copiedFromVersion => undef,
    indexPath => "WebKitTools/Scripts/VCSUtils.pm",
    version => undef,
    # Other values to check
    foundHeaderEnding => 0,
    lastReadLine => undef,
    nextLine => undef,
},
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
    copiedFromVersion => undef,
    indexPath => "WebKitTools/Scripts/VCSUtils.pm",
    version => "53004",
    # Other values to check
    foundHeaderEnding => 1,
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
    copiedFromVersion => undef,
    indexPath => "WebKitTools/Scripts/webkitperl/VCSUtils_unittest/parseDiffHeader.pl",
    version => undef,
    # Other values to check
    foundHeaderEnding => 1,
    lastReadLine => "@@ -0,0 +1,262 @@\n",
    nextLine => "+#!/usr/bin/perl -w\n",
},
{
    # New test
    diffName => "SVN: copy",
    inputText => <<'END',
Index: WebKitTools/Scripts/webkitpy/move_test.py
===================================================================
--- WebKitTools/Scripts/webkitpy/move_test.py	(revision 53047)	(from WebKitTools/Scripts/webkitpy/__init__.py:53048)
+++ WebKitTools/Scripts/webkitpy/move_test.py	(working copy)
@@ -0,0 +1,7 @@
+# Required for Python to search this directory for module files
END
    # Header keys to check
    svnConvertedText => <<'END',
Index: WebKitTools/Scripts/webkitpy/move_test.py
===================================================================
--- WebKitTools/Scripts/webkitpy/move_test.py	(revision 53047)	(from WebKitTools/Scripts/webkitpy/__init__.py:53048)
+++ WebKitTools/Scripts/webkitpy/move_test.py	(working copy)
END
    copiedFromPath => "WebKitTools/Scripts/webkitpy/__init__.py",
    copiedFromVersion => "53048",
    indexPath => "WebKitTools/Scripts/webkitpy/move_test.py",
    version => undef,
    # Other values to check
    foundHeaderEnding => 1,
    lastReadLine => "@@ -0,0 +1,7 @@\n",
    nextLine => "+# Required for Python to search this directory for module files\n",
},
{
    # New test
    diffName => "SVN: \\r\\n lines",
    inputText => <<END, # No single quotes to allow interpolation of "\r"
Index: WebKitTools/Scripts/webkitpy/move_test.py\r
===================================================================\r
--- WebKitTools/Scripts/webkitpy/move_test.py	(revision 53047)	(from WebKitTools/Scripts/webkitpy/__init__.py:53048)\r
+++ WebKitTools/Scripts/webkitpy/move_test.py	(working copy)\r
@@ -0,0 +1,7 @@\r
+# Required for Python to search this directory for module files\r
END
    # Header keys to check
    svnConvertedText => <<END, # No single quotes to allow interpolation of "\r"
Index: WebKitTools/Scripts/webkitpy/move_test.py\r
===================================================================\r
--- WebKitTools/Scripts/webkitpy/move_test.py	(revision 53047)	(from WebKitTools/Scripts/webkitpy/__init__.py:53048)\r
+++ WebKitTools/Scripts/webkitpy/move_test.py	(working copy)\r
END
    copiedFromPath => "WebKitTools/Scripts/webkitpy/__init__.py",
    copiedFromVersion => "53048",
    indexPath => "WebKitTools/Scripts/webkitpy/move_test.py",
    version => undef,
    # Other values to check
    foundHeaderEnding => 1,
    lastReadLine => "@@ -0,0 +1,7 @@\r\n",
    nextLine => "+# Required for Python to search this directory for module files\r\n",
},
{
    # New test
    diffName => "SVN: path corrections",
    inputText => <<'END',
Index: WebKitTools/Scripts/webkitpy/move_test.py
===================================================================
--- bad_path	(revision 53047)	(from WebKitTools/Scripts/webkitpy/__init__.py:53048)
+++ bad_path	(working copy)
@@ -0,0 +1,7 @@
+# Required for Python to search this directory for module files
END
    # Header keys to check
    svnConvertedText => <<'END',
Index: WebKitTools/Scripts/webkitpy/move_test.py
===================================================================
--- WebKitTools/Scripts/webkitpy/move_test.py	(revision 53047)	(from WebKitTools/Scripts/webkitpy/__init__.py:53048)
+++ WebKitTools/Scripts/webkitpy/move_test.py	(working copy)
END
    copiedFromPath => "WebKitTools/Scripts/webkitpy/__init__.py",
    copiedFromVersion => "53048",
    indexPath => "WebKitTools/Scripts/webkitpy/move_test.py",
    version => undef,
    # Other values to check
    foundHeaderEnding => 1,
    lastReadLine => "@@ -0,0 +1,7 @@\n",
    nextLine => "+# Required for Python to search this directory for module files\n",
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
===================================================================
--- WebCore/rendering/style/StyleFlexibleBoxData.h
+++ WebCore/rendering/style/StyleFlexibleBoxData.h
END
    copiedFromPath => undef,
    copiedFromVersion => undef,
    indexPath => "WebCore/rendering/style/StyleFlexibleBoxData.h",
    version => undef,
    # Other values to check
    foundHeaderEnding => 1,
    lastReadLine => "@@ -47,7 +47,6 @@ public:\n",
    nextLine => undef,
},
{
    # New test
    diffName => "Git: unrecognized lines",
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
    copiedFromVersion => undef,
    indexPath => "LayoutTests/http/tests/security/listener/xss-inactive-closure.html",
    version => undef,
    # Other values to check
    foundHeaderEnding => 1,
    lastReadLine => "@@ -0,0 +1,34 @@\n",
    nextLine => "+<html>\n",
},
);

plan(tests => @diffTests * 8); # Eight assertions per call to doDiffTest().

foreach my $diffTestHashRef (@diffTests) {
    doDiffTest($diffTestHashRef);
}
