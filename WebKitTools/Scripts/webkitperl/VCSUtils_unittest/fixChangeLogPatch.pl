#!/usr/bin/perl
#
# Copyright (C) 2009, 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
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
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

# Unit tests of VCSUtils::fixChangeLogPatch().

use Test::Simple tests => 12;
use VCSUtils;

# The source ChangeLog for these tests is the following:
# 
# 2009-12-22  Alice  <alice@email.address>
# 
#         Reviewed by Ray.
# 
#         Changed some code on 2009-12-22.
# 
#         * File:
#         * File2:
# 
# 2009-12-21  Alice  <alice@email.address>
# 
#         Reviewed by Ray.
# 
#         Changed some code on 2009-12-21.
# 
#         * File:
#         * File2:

my $title;
my $in;
my $out;

# New test
$title = "fixChangeLogPatch: [no change] In-place change.";

$in = <<'END';
--- ChangeLog
+++ ChangeLog
@@ -1,5 +1,5 @@
 2010-12-22  Bob  <bob@email.address>
 
-        Reviewed by Sue.
+        Reviewed by Ray.
 
         Changed some code on 2010-12-22.
END

ok(fixChangeLogPatch($in) eq $in, $title);

# New test
$title = "fixChangeLogPatch: [no change] Remove first entry.";

$in = <<'END';
--- ChangeLog
+++ ChangeLog
@@ -1,11 +1,3 @@
-2010-12-22  Bob  <bob@email.address>
-
-        Reviewed by Ray.
-
-        Changed some code on 2010-12-22.
-
-        * File:
-
 2010-12-22  Alice  <alice@email.address>
 
         Reviewed by Ray.
END

ok(fixChangeLogPatch($in) eq $in, $title);

# New test
$title = "fixChangeLogPatch: [no change] Remove entry in the middle.";

$in = <<'END';
--- ChangeLog
+++ ChangeLog
@@@ -7,10 +7,6 @@
 
         * File:
 
-2010-12-22  Bob  <bob@email.address>
-
-        Changed some code on 2010-12-22.
-
 2010-12-22  Alice  <alice@email.address>
 
         Reviewed by Ray.
END

ok(fixChangeLogPatch($in) eq $in, $title);

# New test
$title = "fixChangeLogPatch: [no change] Far apart changes (i.e. more than one chunk).";

$in = <<'END';
--- ChangeLog
+++ ChangeLog
@@ -7,7 +7,7 @@
 
         * File:
 
-2010-12-22  Bob  <bob@email.address>
+2010-12-22  Bobby <bob@email.address>
 
         Changed some code on 2010-12-22.
 
@@ -21,7 +21,7 @@
 
         * File2:
 
-2010-12-21  Bob  <bob@email.address>
+2010-12-21  Bobby <bob@email.address>
 
         Changed some code on 2010-12-21.
END

ok(fixChangeLogPatch($in) eq $in, $title);

# New test
$title = "fixChangeLogPatch: [no change] First line is new line.";

$in = <<'END';
--- ChangeLog
+++ ChangeLog
@@ -1,3 +1,11 @@
+2009-12-22  Bob  <bob@email.address>
+
+        Reviewed by Ray.
+
+        Changed some more code on 2009-12-22.
+
+        * File:
+
 2009-12-22  Alice  <alice@email.address>
 
         Reviewed by Ray.
END

ok(fixChangeLogPatch($in) eq $in, $title);

# New test
$title = "fixChangeLogPatch: [no change] No date string.";

$in = <<'END';
--- ChangeLog
+++ ChangeLog
@@ -6,6 +6,7 @@
 
         * File:
         * File2:
+        * File3:
 
 2009-12-21  Alice  <alice@email.address>
 
END

ok(fixChangeLogPatch($in) eq $in, $title);

# New test
$title = "fixChangeLogPatch: [no change] New entry inserted in middle.";

$in = <<'END';
--- ChangeLog
+++ ChangeLog
@@ -11,6 +11,14 @@
 
         Reviewed by Ray.
 
+        Changed some more code on 2009-12-21.
+
+        * File:
+
+2009-12-21  Alice  <alice@email.address>
+
+        Reviewed by Ray.
+
         Changed some code on 2009-12-21.
 
         * File:
END

ok(fixChangeLogPatch($in) eq $in, $title);

# New test
$title = "fixChangeLogPatch: [no change] New entry inserted earlier in the file, but after an entry with the same author and date.";

$in = <<'END';
--- ChangeLog
+++ ChangeLog
@@ -70,6 +70,14 @@
 
 2009-12-22  Alice  <alice@email.address>
 
+        Reviewed by Sue.
+
+        Changed some more code on 2009-12-22.
+
+        * File:
+
+2009-12-22  Alice  <alice@email.address>
+
         Reviewed by Ray.
 
         Changed some code on 2009-12-22.
END

ok(fixChangeLogPatch($in) eq $in, $title);

# New test
$title = "fixChangeLogPatch: Leading context includes first line.";

$in = <<'END';
--- ChangeLog
+++ ChangeLog
@@ -1,5 +1,13 @@
 2009-12-22  Alice  <alice@email.address>
 
+        Reviewed by Sue.
+
+        Changed some more code on 2009-12-22.
+
+        * File:
+
+2009-12-22  Alice  <alice@email.address>
+
         Reviewed by Ray.
 
         Changed some code on 2009-12-22.
END

$out = <<'END';
--- ChangeLog
+++ ChangeLog
@@ -1,3 +1,11 @@
+2009-12-22  Alice  <alice@email.address>
+
+        Reviewed by Sue.
+
+        Changed some more code on 2009-12-22.
+
+        * File:
+
 2009-12-22  Alice  <alice@email.address>
 
         Reviewed by Ray.
END

ok(fixChangeLogPatch($in) eq $out, $title);

# New test
$title = "fixChangeLogPatch: Leading context does not include first line.";

$in = <<'END';
@@ -2,6 +2,14 @@
 
         Reviewed by Ray.
 
+        Changed some more code on 2009-12-22.
+
+        * File:
+
+2009-12-22  Alice  <alice@email.address>
+
+        Reviewed by Ray.
+
         Changed some code on 2009-12-22.
 
         * File:
END

$out = <<'END';
@@ -1,3 +1,11 @@
+2009-12-22  Alice  <alice@email.address>
+
+        Reviewed by Ray.
+
+        Changed some more code on 2009-12-22.
+
+        * File:
+
 2009-12-22  Alice  <alice@email.address>
 
         Reviewed by Ray.
END

ok(fixChangeLogPatch($in) eq $out, $title);

# New test
$title = "fixChangeLogPatch: Non-consecutive line additions.";

# This can occur, for example, if the new ChangeLog entry includes
# trailing white space in the first blank line but not the second.
# A diff command can then match the second blank line of the new
# ChangeLog entry with the first blank line of the old.
# The svn diff command with the default --diff-cmd has done this.
$in = <<'END';
@@ -1,5 +1,11 @@
 2009-12-22  Alice  <alice@email.address>
+ <pretend-whitespace>
+        Reviewed by Ray.
 
+        Changed some more code on 2009-12-22.
+
+2009-12-22  Alice  <alice@email.address>
+
         Reviewed by Ray.
 
         Changed some code on 2009-12-22.
END

$out = <<'END';
@@ -1,3 +1,9 @@
+2009-12-22  Alice  <alice@email.address>
+ <pretend-whitespace>
+        Reviewed by Ray.
+
+        Changed some more code on 2009-12-22.
+
 2009-12-22  Alice  <alice@email.address>
 
         Reviewed by Ray.
END

ok(fixChangeLogPatch($in) eq $out, $title);

# New test
$title = "fixChangeLogPatch: Additional edits after new entry.";

$in = <<'END';
@@ -2,10 +2,17 @@
 
         Reviewed by Ray.
 
+        Changed some more code on 2009-12-22.
+
+        * File:
+
+2009-12-22  Alice  <alice@email.address>
+
+        Reviewed by Ray.
+
         Changed some code on 2009-12-22.
 
         * File:
-        * File2:
 
 2009-12-21  Alice  <alice@email.address>
 
END

$out = <<'END';
@@ -1,11 +1,18 @@
+2009-12-22  Alice  <alice@email.address>
+
+        Reviewed by Ray.
+
+        Changed some more code on 2009-12-22.
+
+        * File:
+
 2009-12-22  Alice  <alice@email.address>
 
         Reviewed by Ray.
 
         Changed some code on 2009-12-22.
 
         * File:
-        * File2:
 
 2009-12-21  Alice  <alice@email.address>
 
END

ok(fixChangeLogPatch($in) eq $out, $title);
