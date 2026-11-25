#!/usr/bin/env perl

# Copyright (C) 2026 Apple Inc. All rights reserved.
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

# Unit tests for applyPreprocessor() in Source/WebCore/bindings/scripts/preprocessor.pm.

use strict;
use warnings;

use File::Spec;
use File::Temp qw(tempfile);
use FindBin;
use Test::More;

use lib File::Spec->catdir($FindBin::Bin, "..", "..", "..", "..", "Source", "WebCore", "bindings", "scripts");
require preprocessor;

sub preprocess
{
    my ($content, $defines) = @_;
    my ($fh, $fileName) = tempfile(SUFFIX => ".idl", UNLINK => 1);
    print $fh $content;
    close($fh);
    return join("", applyPreprocessor($fileName, $defines));
}

my $sarGuard = "#ifdef RC_REMOVE_SEC_ACCEL_5\nold\n#else\nnew\n#endif // RC_REMOVE_SEC_ACCEL_5\n";
is(preprocess($sarGuard, "RC_REMOVE_SEC_ACCEL_5"), "old\n", "#ifdef keeps its block when the macro is defined");
is(preprocess($sarGuard, ""), "new\n", "#ifdef takes #else when the macro is undefined");

my $ifndef = "#ifndef RC_REMOVE_SEC_ACCEL_5\nnew\n#endif\n";
is(preprocess($ifndef, ""), "new\n", "#ifndef keeps its block when the macro is undefined");
is(preprocess($ifndef, "RC_REMOVE_SEC_ACCEL_5"), "", "#ifndef drops its block when the macro is defined");

my $ifdefElif = "#ifdef A\na\n#elif defined(B)\nb\n#else\nc\n#endif\n";
is(preprocess($ifdefElif, "B"), "b\n", "#elif follows a false #ifdef");
is(preprocess($ifdefElif, "A B"), "a\n", "#elif is skipped after a true #ifdef");

is(preprocess("#if defined(X) && X\n#ifdef Y\ninner\n#endif\n#endif\nafter\n", "Y"), "after\n",
    "#ifdef nested in a false #if stays dropped");

my $notDefined = "#if !defined(RC_REMOVE_SEC_ACCEL_5)\nnew\n#else\nold\n#endif\n";
is(preprocess($notDefined, "RC_REMOVE_SEC_ACCEL_5"), "old\n", "!defined(X) is false when X is defined");
is(preprocess($notDefined, ""), "new\n", "!defined(X) is true when X is undefined");
is(preprocess("#if !X\nnew\n#endif\n", "X"), "", "!X is false when X is defined");
is(preprocess("#if defined(A) && !defined(B)\nab\n#endif\n", "A B"), "", "!defined(X) combines with other terms");
is(preprocess("#if defined(A) && !defined(B)\nab\n#endif\n", "A"), "ab\n", "!defined(X) in a conjunction is true when X is undefined");
is(preprocess("#if !defined(RC_REMOVE_SEC_ACCEL_5) // fixed\nnew\n#else\nold\n#endif\n", "RC_REMOVE_SEC_ACCEL_5"), "old\n",
    "a trailing comment does not change a condition");
is(preprocess("#if defined(X) && X\nx\n#elif !defined(RC_REMOVE_SEC_ACCEL_5) // fixed\nnew\n#endif\n", ""), "new\n",
    "a trailing comment does not change an #elif condition");

my $existingForms = <<'END';
#if defined(ENABLE_FOO) && ENABLE_FOO
foo
#endif
#if defined(ENABLE_BAR) && ENABLE_BAR
bar
#else
notBar
#endif
#if !(defined(ENABLE_FOO) && ENABLE_FOO)
notFoo
#endif
#ifdef RC_REMOVE_SEC_ACCEL_5
original
#else
fixed
#endif // RC_REMOVE_SEC_ACCEL_5
#ifndef RC_REMOVE_SEC_ACCEL_5
added
#endif
#if defined(RC_REMOVE_SEC_ACCEL_5) && RC_REMOVE_SEC_ACCEL_5
originalToo
#endif
END
is(preprocess($existingForms, "ENABLE_FOO"), "foo\nnotBar\nfixed\nadded\n",
    "existing directive forms without RC_REMOVE_SEC_ACCEL_5");
is(preprocess($existingForms, "ENABLE_FOO RC_REMOVE_SEC_ACCEL_5"), "foo\nnotBar\noriginal\noriginalToo\n",
    "existing directive forms with RC_REMOVE_SEC_ACCEL_5");

for my $condition ("defined(A) || defined(B)", "ENABLE(FOO)", "RELEASE_SECURITY_UPDATE(6)", "VERSION >= 2", "defined A") {
    eval { preprocess("#if $condition\nnew\n#endif\n", "A B ENABLE_FOO VERSION") };
    like($@, qr/cannot evaluate .*\Q$condition\E/, "an unrecognized condition stops preprocessing: $condition");
}
eval { preprocess("#if defined(A) && A\na\n#elif defined(B) || defined(C)\nb\n#endif\n", "") };
like($@, qr/cannot evaluate .*\Qdefined(B) || defined(C)\E/, "an unrecognized #elif condition stops preprocessing when the #elif is reached");
eval { preprocess("#if defined(A) && A && defined(B) || defined(C)\nnew\n#endif\n", "") };
like($@, qr/cannot evaluate .*\Qdefined(B) || defined(C)\E/, "an unrecognized term after a false term stops preprocessing");

is(preprocess("#if defined(A) && A\na\n#elif defined(B) || defined(C)\nb\n#endif\n", "A"), "a\n",
    "an #elif after a taken branch is not evaluated");
is(preprocess("#if defined(X) && X\n#if defined(A) || defined(B)\na\n#elif VERSION >= 2\nb\n#else\nc\n#endif\n#endif\nafter\n", "A"), "after\n",
    "unrecognized conditions inside an excluded block are not evaluated");

done_testing();
