# Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

package Bugzilla::Extension::Commits;

use strict;
use warnings;

use parent qw(Bugzilla::Extension);

our $VERSION = "1.0.0";

sub bug_format_comment {
    my ($self, $args) = @_;
    my $regexes = $args->{'regexes'};

    # Should match "r12345" and "trac.webkit.org/r12345" but not "https://trac.webkit.org/r12345"
    push(@$regexes, { match => qr/(?<!\/|\#)\b((r[[:digit:]]{5,}))\b/, replace => \&_replace_reference });
    push(@$regexes, { match => qr/(?<!\/)(trac.webkit.org\/(r[[:digit:]]{5,}))\b/, replace => \&_replace_reference });
    push(@$regexes, { match => qr/\b((?<!https:\/\/)(?<!\/|\x{2026}|\.)(\d+@\S+))\b/, replace => \&_replace_reference });
    push(@$regexes, { match => qr/\b((?<!https:\/\/)(?<!\/|\x{2026}|\.)(\d+\.?\d+@\S+))\b/, replace => \&_replace_reference });
}

sub _replace_reference {
    my $args = shift;
    my $text = $args->{matches}->[0];
    my $reference = $args->{matches}->[1];
    return qq{<a href="https://commits.webkit.org/$reference">$text</a>};
};

# This must be the last line of your extension.
__PACKAGE__->NAME;
