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

# Unit tests of VCSUtils::parseGitFileMode().

use Test::Simple tests => 6;
use VCSUtils;

my $title;
my $patchLine;

# New test
$title = "parseGitFileMode: Parse a valid Git file mode for new file.";

$patchLine = "new file mode 100755";
ok(parseGitFileMode($patchLine) == 100755, $title);

# New test
$title = "parseGitFileMode: Parse a valid Git file mode for an existing file.";

$patchLine = "new mode 100755";
ok(parseGitFileMode($patchLine) == 100755, $title);

# New test
$title = "parseGitFileMode: Parse an invalid Git file mode for new file.";

$patchLine = "new file mode dummy";
ok(!parseGitFileMode($patchLine), $title);

# New test
$title = "parseGitFileMode: Parse an invalid Git file mode for an existing file.";

$patchLine = "new mode dummy";
ok(!parseGitFileMode($patchLine), $title);

# New test
$title = "parseGitFileMode: Ensure that a numeric file mode longer than 6 digits is invalid for a new file.";

$patchLine = "new file mode 1007559";
ok(!parseGitFileMode($patchLine), $title);

# New test
$title = "parseGitFileMode: Ensure that a numeric file mode longer than 6 digits is invalid for an existing file.";

$patchLine = "new mode 1007559";
ok(!parseGitFileMode($patchLine), $title);
