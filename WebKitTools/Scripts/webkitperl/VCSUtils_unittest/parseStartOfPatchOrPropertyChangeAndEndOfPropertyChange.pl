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

# Unit tests of VCSUtils::parseStartOfPatch() and VCSUtils::parsePropertyChange()
# and VCSUtils::isEndOfPropertyChange.

use Test::Simple tests => 6;
use VCSUtils;

my $title;

# New test
$title = "parseStartOfPatch: Test start of SVN patch.";
ok(parseStartOfPatch("Index: file_to_patch.txt\n") eq "file_to_patch.txt", $title);

# New test
$title = "parsePropertyChange: Test start of SVN property change entry.";
ok(parsePropertyChange("Property changes on: file_to_patch.txt\r\n") eq "file_to_patch.txt", $title);

# New test
$title = "isEndOfPropertyChange: Test end of SVN property change entry addition.";
ok(isEndOfPropertyChange("   + *\n", "file_to_patch.txt"), $title);

# New test
$title = "isEndOfPropertyChange: Test end of SVN property change entry deletion.";
ok(isEndOfPropertyChange("   - *\n", "file_to_patch.txt"), $title);

# New test
$title = "isEndOfPropertyChange: Test end of SVN property change entry addition with invalid property change path.";
ok(!isEndOfPropertyChange("   + *\n", 0), $title);

# New test
$title = "isEndOfPropertyChange: Test end of SVN property change entry deletion with invalid property change path.";
ok(!isEndOfPropertyChange("   - *\n", 0), $title);
