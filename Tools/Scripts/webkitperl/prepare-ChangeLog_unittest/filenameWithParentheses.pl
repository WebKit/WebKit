#!/usr/bin/env perl
#
# Copyright (C) 2020 Google Inc. All rights reserved.
#   
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#   1.  Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#   2.  Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#   
#   THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
#   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#   ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC. OR ITS CONTRIBUTORS BE LIABLE
#   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
#   DAMAGE.

use strict;
use warnings;

use File::Temp;
use FindBin;
use Test::More;
use VCSUtils;

use lib File::Spec->catdir($FindBin::Bin, "..");
use LoadAsModule qw(PrepareChangeLog prepare-ChangeLog);

plan(tests => 1);

sub writeFileWithContent($$)
{
    my ($file, $content) = @_;
    open(FILE, ">", $file) or die "Cannot open $file: $!";
    print FILE $content;
    close(FILE);
}

my $temporaryDirectory = File::Temp->newdir();
system("git", "init", $temporaryDirectory);
my $filename = File::Spec->catfile($temporaryDirectory, "FileWith(Parentheses).txt");
writeFileWithContent($filename, "");
writeFileWithContent(File::Spec->catfile($temporaryDirectory, ".gitattributes"), "* foo=1\nb");

my $result = 0;
chdir $temporaryDirectory;
$result = PrepareChangeLog::attributeCommand($filename, "foo");
chdir '..';

is($result, 1, "Should successfully get foo attribute from file with parentheses in name.");
