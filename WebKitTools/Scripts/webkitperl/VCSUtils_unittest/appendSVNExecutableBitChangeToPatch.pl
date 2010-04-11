#!/usr/bin/perl
#
# Copyright (C) Research in Motion Limited 2010. All rights reserved.
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
#     * Neither the name of Research in Motion Limited nor the names of its
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

# Unit tests of VCSUtils::appendSVNExecutableBitChangeToPatch().

use Test::Simple tests => 2;
use VCSUtils;

my $title;
my $patch;
my $expectedPatch;

# New test
$title = "appendSVNExecutableBitChangeToPatch: Add executable bit to file.";

$expectedPatch = <<'END';
Property changes on: file_to_patch.txt
___________________________________________________________________
Added: svn:executable
   + *
END

$patch = "";
appendSVNExecutableBitChangeToPatch("file_to_patch.txt", "100755", \$patch);

ok($patch eq $expectedPatch, $title);

# New test
$title = "appendSVNExecutableBitChangeToPatch: Remove executable bit from file.";

$expectedPatch = <<'END';
Property changes on: file_to_patch.txt
___________________________________________________________________
Deleted: svn:executable
   + *
END

$patch = "";
appendSVNExecutableBitChangeToPatch("file_to_patch.txt", "100644", \$patch);

ok($patch eq $expectedPatch, $title);
