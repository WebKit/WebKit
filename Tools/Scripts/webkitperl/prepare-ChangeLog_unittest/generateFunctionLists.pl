#!/usr/bin/env perl
#
# Copyright (C) 2016 Apple Inc. All rights reserved.
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

use strict;
use warnings;

use File::Temp;
use FindBin;
use Test::More;
use VCSUtils;

use lib File::Spec->catdir($FindBin::Bin, "..");
use LoadAsModule qw(PrepareChangeLog prepare-ChangeLog);

my $EXAMPLE_CPP = <<EOF;
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>

unsigned fib(unsigned);

unsigned fibPlusFive(unsigned n)
{
    return fib(n) + 5;
}

unsigned fib(unsigned n)
{
    // Derived from Binet's formula, <http://mathworld.wolfram.com/BinetsFibonacciNumberFormula.html>.
    assert(n <= std::floor(std::log(std::numeric_limits<unsigned>::max() * std::sqrt(5)) / std::log((1 + std::sqrt(5)) / 2)));
    if (!n)
        return 0;
    if (n == 1)
        return 1;
    --n;
    unsigned a = 0;
    unsigned b = 1;
    do {
        unsigned k = a + b;
        a = b;
        b = k;
    } while (--n);
    return b;
}

int main(int argc, const char* argv[])
{
    std::cout << fib(1) << std::endl;
    std::cout << fibPlusFive(1) << std::endl;
    std::cout << fib(5) << std::endl;
    std::cout << fibPlusFive(5) << std::endl;
    return 0;
}
EOF

my $EXAMPLE_JAVASCRIPT_WITH_SAME_FUNCTION_DEFINED_TWICE = <<EOF;
function f() {
    return 5;
}

function f() {
    return 6;
}

function g() {
    return 7;
}
EOF

my @testCaseHashRefs = (
###
# 0 lines of context
##
{
    testName => "Unified diff with 0 context",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -15 +14,0 @@ unsigned fib(unsigned n)
-    // Derived from Binet's formula, <http://mathworld.wolfram.com/BinetsFibonacciNumberFormula.html>.
EOF
    expected => <<EOF

        (fib):
EOF
},
{
    testName => "Unified diff with 0 context; combine two adjacent functions",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -10,5 +9,0 @@ unsigned fibPlusFive(unsigned n)
-    return fib(n) + 5;
-}
-
-unsigned fib(unsigned n)
-{
EOF
    expected => <<EOF

        (fibPlusFive):
        (fib): Deleted.
EOF
},
{
    testName => "Unified diff with 0 context; remove function",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -13,19 +12,0 @@ unsigned fibPlusFive(unsigned n)
-unsigned fib(unsigned n)
-{
-    // Derived from Binet's formula, <http://mathworld.wolfram.com/BinetsFibonacciNumberFormula.html>.
-    assert(n <= std::floor(std::log(std::numeric_limits<unsigned>::max() * std::sqrt(5)) / std::log((1 + std::sqrt(5)) / 2)));
-    if (!n)
-        return 0;
-    if (n == 1)
-        return 1;
-    --n;
-    unsigned a = 0;
-    unsigned b = 1;
-    do {
-        unsigned k = a + b;
-        a = b;
-        b = k;
-    } while (--n);
-    return b;
-}
-
EOF
    expected => <<EOF

        (fib): Deleted.
EOF
},
{
    testName => "Unified diff with 0 context; add function immediately after another function",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -11,0 +12,4 @@ unsigned fibPlusFive(unsigned n)
+unsigned fibPlusSeven(unsigned n)
+{
+    return fib(n) + 7;
+}
EOF
    expected => <<EOF

        (fibPlusSeven):
EOF
},
{
    testName => "Unified diff with 0 context; add function at the end of the file",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -39,0 +40,5 @@ int main(int argc, const char* argv[])
+
+unsigned fibPlusSeven(unsigned n)
+{
+    return fib(n) + 7;
+}
EOF
    expected => <<EOF

        (fibPlusSeven):
EOF
},
{
    testName => "Unified diff with 0 context; rename function",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -8 +8 @@ unsigned fib(unsigned);
-unsigned fibPlusFive(unsigned n)
+unsigned fibPlusFive2(unsigned n)
EOF
    expected => <<EOF

        (fibPlusFive2):
        (fibPlusFive): Deleted.
EOF
},
{
    testName => "Unified diff with 0 context; replace function",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -8,4 +8,5 @@ unsigned fib(unsigned);
-unsigned fibPlusFive(unsigned n)
-{
-    return fib(n) + 5;
-}
+unsigned fibPlusSeven(unsigned n)
+{   // First comment
+    // Second comment
+    return fib(n) + 7;
+};
EOF
    expected => <<EOF

        (fibPlusSeven):
        (fibPlusFive): Deleted.
EOF
},
{
    testName => "Unified diff with 0 context; move function from the top of the file to the end of the file",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -8,5 +7,0 @@ unsigned fib(unsigned);
-unsigned fibPlusFive(unsigned n)
-{
-    return fib(n) + 5;
-}
-
@@ -39,0 +35,5 @@ int main(int argc, const char* argv[])
+
+unsigned fibPlusFive(unsigned n)
+{
+    return fib(n) + 5;
+}
EOF
    expected => <<EOF

        (fibPlusFive):
EOF
},
{
    testName => "Unified diff with 0 context; remove functions with the same name",
    inputText => $EXAMPLE_JAVASCRIPT_WITH_SAME_FUNCTION_DEFINED_TWICE,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -1,8 +0,0 @@
-function f() {
-    return 5;
-}
-
-function f() {
-    return 6;
-}
-
EOF
    expected => <<EOF

        (f): Deleted.
EOF
},
{
    # This is a contrived example and would cause a redefinition error in C++, but an analagous change in a JavaScript script would be fine.
    testName => "Unified diff with 0 context; add function above function with the same name",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -33,0 +34,5 @@ int main(int argc, const char* argv[])
+    return 0;
+}
+
+int main(int argc, const char* argv[])
+{
EOF
    expected => <<EOF

        (main):
EOF
},
{
    # This is a contrived example and would cause a redefinition error in C++, but an analagous change in a JavaScript script would be fine.
    testName => "Unified diff with 0 context; add function after function with the same name",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -38,0 +39,5 @@ int main(int argc, const char* argv[])
+}
+
+int main(int argc, const char* argv[])
+{
+    return 0;
EOF
    expected => <<EOF

        (main):
EOF
},
###
# 3 lines of context
##
{
    testName => "Unified diff with 3 lines of context",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -12,7 +12,6 @@ unsigned fibPlusFive(unsigned n)
 
 unsigned fib(unsigned n)
 {
-    // Derived from Binet's formula, <http://mathworld.wolfram.com/BinetsFibonacciNumberFormula.html>.
     assert(n <= std::floor(std::log(std::numeric_limits<unsigned>::max() * std::sqrt(5)) / std::log((1 + std::sqrt(5)) / 2)));
     if (!n)
         return 0;
EOF
    expected => <<EOF

        (fib):
EOF
},
{
    testName => "Unified diff with 3 lines of context; combine two adjacent functions",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -7,11 +7,6 @@ unsigned fib(unsigned);
 
 unsigned fibPlusFive(unsigned n)
 {
-    return fib(n) + 5;
-}
-
-unsigned fib(unsigned n)
-{
     // Derived from Binet's formula, <http://mathworld.wolfram.com/BinetsFibonacciNumberFormula.html>.
     assert(n <= std::floor(std::log(std::numeric_limits<unsigned>::max() * std::sqrt(5)) / std::log((1 + std::sqrt(5)) / 2)));
     if (!n)
EOF
    expected => <<EOF,

        (fibPlusFive):
        (fib): Deleted.
EOF
},
{
    testName => "Unified diff with 3 lines of context; remove function",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -10,25 +10,6 @@ unsigned fibPlusFive(unsigned n)
     return fib(n) + 5;
 }
 
-unsigned fib(unsigned n)
-{
-    // Derived from Binet's formula, <http://mathworld.wolfram.com/BinetsFibonacciNumberFormula.html>.
-    assert(n <= std::floor(std::log(std::numeric_limits<unsigned>::max() * std::sqrt(5)) / std::log((1 + std::sqrt(5)) / 2)));
-    if (!n)
-        return 0;
-    if (n == 1)
-        return 1;
-    --n;
-    unsigned a = 0;
-    unsigned b = 1;
-    do {
-        unsigned k = a + b;
-        a = b;
-        b = k;
-    } while (--n);
-    return b;
-}
-
 int main(int argc, const char* argv[])
 {
     std::cout << fib(1) << std::endl;
EOF
    # Note that this output is expected given that the chunk range includes the removed content plus context lines.
    # The context lines make the after chunk range (+10, 6) overlap the implementation of fibPlusFive() and main().
    expected => <<EOF,

        (fibPlusFive):
        (main):
        (fib): Deleted.
EOF
},
###
# Diff that does not differ with respect to number of context lines
##
{
    testName => "Unified diff; delete contents of entire file",
    inputText => $EXAMPLE_CPP,
    diffToApply => <<EOF,
diff --git a/fib.cpp b/fib.cpp
--- a/fib.cpp
+++ b/fib.cpp
@@ -1,39 +0,0 @@
-#include <cassert>
-#include <cmath>
-#include <limits>
-#include <iostream>
-
-unsigned fib(unsigned);
-
-unsigned fibPlusFive(unsigned n)
-{
-    return fib(n) + 5;
-}
-
-unsigned fib(unsigned n)
-{
-    // Derived from Binet's formula, <http://mathworld.wolfram.com/BinetsFibonacciNumberFormula.html>.
-    assert(n <= std::floor(std::log(std::numeric_limits<unsigned>::max() * std::sqrt(5)) / std::log((1 + std::sqrt(5)) / 2)));
-    if (!n)
-        return 0;
-    if (n == 1)
-        return 1;
-    --n;
-    unsigned a = 0;
-    unsigned b = 1;
-    do {
-        unsigned k = a + b;
-        a = b;
-        b = k;
-    } while (--n);
-    return b;
-}
-
-int main(int argc, const char* argv[])
-{
-    std::cout << fib(1) << std::endl;
-    std::cout << fibPlusFive(1) << std::endl;
-    std::cout << fib(5) << std::endl;
-    std::cout << fibPlusFive(5) << std::endl;
-    return 0;
-}
EOF
    expected => <<EOF

        (fibPlusFive): Deleted.
        (fib): Deleted.
        (main): Deleted.
EOF
},
);

sub discardOutput(&)
{
    my ($functionRef) = @_;
    open(my $savedStdout, ">&STDOUT") or die "Cannot dup STDOUT: $!";
    open(my $savedStderr, ">&STDERR") or die "Cannot dup STDERR: $!";

    open(STDOUT, ">", File::Spec->devnull());
    open(STDERR, ">", File::Spec->devnull());
    &$functionRef();

    open(STDOUT, ">&", $savedStdout) or die "Cannot restore stdout: $!";
    open(STDERR, ">&", $savedStderr) or die "Cannot restore stderr: $!";
}

sub writeFileWithContent($$)
{
    my ($file, $content) = @_;
    open(FILE, ">", $file) or die "Cannot open $file: $!";
    print FILE $content;
    close(FILE);
}

my $testCasesCount = @testCaseHashRefs;
plan(tests => $testCasesCount);

my $temporaryDirectory = File::Temp->newdir();
my $filename = "fib.cpp";
my $originalFile = File::Spec->catfile($temporaryDirectory, $filename);
my $patchedFile = File::Spec->catfile($temporaryDirectory, "patched-$filename");
my $diffFile = File::Spec->catfile($temporaryDirectory, "a.diff");
foreach my $testCase (@testCaseHashRefs) {
    writeFileWithContent($originalFile, $testCase->{inputText});
    writeFileWithContent($diffFile, $testCase->{diffToApply});
    my $exitCode = exitStatus(system("patch", "-s", "-d", $temporaryDirectory, "-i", $diffFile, "-o", $patchedFile));
    die "Failed to apply patch for $testCase->{testName}: $exitCode" if $exitCode;

    my %delegateHash = (
        openDiff => sub () {
            my $fileHandle;
            open($fileHandle, "<", \$testCase->{diffToApply});
            return $fileHandle;
        },
        openFile => sub () {
            return unless open(PATCHED_FILE, "<", $patchedFile);
            return \*PATCHED_FILE;
        },
        openOriginalFile => sub () {
            my $fileHandle;
            open($fileHandle, "<", \$testCase->{inputText});
            return $fileHandle;
        },
        normalizePath => sub () {
            return $filename;
        },
    );
    my %functionLists;
    discardOutput(sub {
        PrepareChangeLog::actuallyGenerateFunctionLists([$filename], \%functionLists, undef, undef, undef, \%delegateHash);
    });
    chomp(my $expected = $testCase->{expected});
    is($functionLists{$filename}, $expected, $testCase->{testName});
}
