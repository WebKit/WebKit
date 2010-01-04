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

# Unit tests of VCSUtils.pm.

use Test::Simple tests => 21;

use FindBin;
use lib $FindBin::Bin; # so this script can be run from any directory.

use VCSUtils;

# Call a function while suppressing STDERR.
sub callSilently($@) {
    my ($func, @args) = @_;

    open(OLDERR, ">&STDERR");
    close(STDERR);
    my @returnValue = &$func(@args);
    open(STDERR, ">&OLDERR");
    close(OLDERR); # FIXME: Is this necessary?

    return @returnValue;
}

# fixChangeLogPatch
#
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

# Tests: generateRunPatchCommand

# New test
$title = "generateRunPatchCommand: Undefined optional arguments.";

my $argsHashRef;
my ($patchCommand, $isForcing) = VCSUtils::generateRunPatchCommand($argsHashRef);

ok($patchCommand eq "patch -p0", $title);
ok($isForcing == 0, $title);

# New test
$title = "generateRunPatchCommand: Undefined options.";

my $options;
$argsHashRef = {options => $options};
($patchCommand, $isForcing) = VCSUtils::generateRunPatchCommand($argsHashRef);

ok($patchCommand eq "patch -p0", $title);
ok($isForcing == 0, $title);

# New test
$title = "generateRunPatchCommand: --force and no \"ensure force\".";

$argsHashRef = {options => ["--force"]};
($patchCommand, $isForcing) = VCSUtils::generateRunPatchCommand($argsHashRef);

ok($patchCommand eq "patch -p0 --force", $title);
ok($isForcing == 1, $title);

# New test
$title = "generateRunPatchCommand: no --force and \"ensure force\".";

$argsHashRef = {ensureForce => 1};
($patchCommand, $isForcing) = VCSUtils::generateRunPatchCommand($argsHashRef);

ok($patchCommand eq "patch -p0 --force", $title);
ok($isForcing == 1, $title);

# New test
$title = "generateRunPatchCommand: \"should reverse\".";

$argsHashRef = {shouldReverse => 1};
($patchCommand, $isForcing) = VCSUtils::generateRunPatchCommand($argsHashRef);

ok($patchCommand eq "patch -p0 --reverse", $title);

# New test
$title = "generateRunPatchCommand: --fuzz=3, --force.";

$argsHashRef = {options => ["--fuzz=3", "--force"]};
($patchCommand, $isForcing) = VCSUtils::generateRunPatchCommand($argsHashRef);

ok($patchCommand eq "patch -p0 --force --fuzz=3", $title);

# Tests: runPatchCommand

# New test
$title = "runPatchCommand: Unsuccessful patch, forcing.";

# Since $patch has no "Index:" path, passing this to runPatchCommand
# should not affect any files.
my $patch = <<'END';
Garbage patch contents
END

# We call via callSilently() to avoid output like the following to STDERR:
# patch: **** Only garbage was found in the patch input.
$argsHashRef = {ensureForce => 1};
$exitStatus = callSilently(\&runPatchCommand, $patch, ".", "file_to_patch.txt", $argsHashRef);

ok($exitStatus != 0, $title);

# New test
$title = "runPatchCommand: New file, --dry-run.";

# This file should not exist after the tests, but we take care with the
# file name and contents just in case.
my $fileToPatch = "temp_OK_TO_ERASE__README_FOR_MORE.txt";
$patch = <<END;
Index: $fileToPatch
===================================================================
--- $fileToPatch	(revision 0)
+++ $fileToPatch	(revision 0)
@@ -0,0 +1,5 @@
+This is a test file for WebKitTools/Scripts/VCSUtils_unittest.pl.
+This file should not have gotten created on your system.
+If it did, some unit tests don't seem to be working quite right:
+It would be great if you could file a bug report. Thanks!
+---------------------------------------------------------------------
END

# --dry-run prevents creating any files.
# --silent suppresses the success message to STDOUT.
$argsHashRef = {options => ["--dry-run", "--silent"]};
$exitStatus = runPatchCommand($patch, ".", $fileToPatch, $argsHashRef);

ok($exitStatus == 0, $title);

# New test
$title = "runPatchCommand: New file: \"$fileToPatch\".";

$argsHashRef = {options => ["--silent"]};
$exitStatus = runPatchCommand($patch, ".", $fileToPatch, $argsHashRef);

ok($exitStatus == 0, $title);

# New test
$title = "runPatchCommand: Reverse new file (clean up previous).";

$argsHashRef = {shouldReverse => 1,
                options => ["--silent", "--remove-empty-files"]}; # To clean up.
$exitStatus = runPatchCommand($patch, ".", $fileToPatch, $argsHashRef);
ok($exitStatus == 0, $title);
