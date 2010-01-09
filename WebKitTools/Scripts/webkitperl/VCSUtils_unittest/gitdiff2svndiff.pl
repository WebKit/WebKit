#!/usr/bin/perl -w
#
# Copyright (C) 2009, 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies) 
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

# Unit tests of VCSUtils::gitdiff2svndiff()

use strict;
use warnings;

use Test::Simple tests => 20;
use VCSUtils;

# We use this for display purposes, to keep each test title on one line.
sub excerptString($)
{
    my ($text) = @_;
    
    my $length = 25;
    
    my $shortened = substr($text, 0, $length);
    $shortened .= "..." if (length($text) > $length);
    
    return $shortened;
}

my $git_patch = <<END;
diff --git a/WebCore/rendering/style/StyleFlexibleBoxData.h b/WebCore/rendering/style/StyleFlexibleBoxData.h
index f5d5e74..3b6aa92 100644
--- a/WebCore/rendering/style/StyleFlexibleBoxData.h
+++ b/WebCore/rendering/style/StyleFlexibleBoxData.h
@@ -47,7 +47,6 @@ public:
END

my $svn_patch = <<END;
Index: WebCore/rendering/style/StyleFlexibleBoxData.h
===================================================================
--- WebCore/rendering/style/StyleFlexibleBoxData.h
+++ WebCore/rendering/style/StyleFlexibleBoxData.h
@@ -47,7 +47,6 @@ public:
END

my @gitLines = split("\n", $git_patch);
my @svnLines = split("\n", $svn_patch);

# New test: check each git header line with different line endings
my $titleHeader = "gitdiff2svndiff: ";

my @lineEndingPairs = ( # display name, value
    ["", ""],
    ["\\n", "\n"],
    ["\\r\\n", "\r\n"],
);

for (my $i = 0; $i < @gitLines; $i++) {
    foreach my $pair (@lineEndingPairs) { 
        my $gitLine = $gitLines[$i] . $pair->[1];
        my $expected = $svnLines[$i] . $pair->[1];
        my $title = $titleHeader . excerptString($gitLine);
        $title .= " [line-end: \"$pair->[0]\"]";
        
        ok($expected eq gitdiff2svndiff($gitLine), $title);
    }
}

# New test
my $title = "gitdiff2svndiff: Convert mnemonic git diff to svn diff";

my @prefixes = (
    { 'a' => 'i', 'b' => 'w' }, # git-diff (compares the (i)ndex and the (w)ork tree)
    { 'a' => 'c', 'b' => 'w' }, # git-diff HEAD (compares a (c)ommit and the (w)ork tree)
    { 'a' => 'c', 'b' => 'i' }, # git diff --cached (compares a (c)ommit and the (i)ndex)
    { 'a' => 'o', 'b' => 'w' }, # git-diff HEAD:file1 file2 (compares an (o)bject and a (w)ork tree entity)
    { 'a' => '1', 'b' => '2' }, # git diff --no-index a b (compares two non-git things (1) and (2))
);

my $out = "";

foreach my $prefix (@prefixes) {
    my $mnemonic_patch = $git_patch;
    $mnemonic_patch =~ s/ a\// $prefix->{'a'}\//g;
    $mnemonic_patch =~ s/ b\// $prefix->{'b'}\//g;

    $out = "";
    foreach my $line (split('\n', $mnemonic_patch)) {
        $out .= gitdiff2svndiff($line) . "\n";
    }

    ok($svn_patch eq $out, $title . " (" . $prefix->{'a'} . "," . $prefix->{'b'} . ")");
}

