#!/usr/bin/perl -w

# Copyright (C) 2011 Adam Barth <abarth@webkit.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
#

use strict;

use IO::File;
use InFilesParser;

package InFilesCompiler;

my $defaultItemFactory;

my %parsedItems;
my %parsedParameters;

sub itemHandler($$$)
{
    my ($itemName, $property, $value) = @_;

    $parsedItems{$itemName} = { &$defaultItemFactory($itemName) } if !defined($parsedItems{$itemName});

    return unless $property;

    die "Unknown property $property for $itemName\n" if !defined($parsedItems{$itemName}{$property});
    $parsedItems{$itemName}{$property} = $value;
}

sub parameterHandler($$)
{
    my ($parameter, $value) = @_;

    die "Unknown parameter $parameter\n" if !defined($parsedParameters{$parameter});
    $parsedParameters{$parameter} = $value;
}

sub new()
{
    my $object = shift;
    my $reference = { };

    my $defaultParametersRef = shift;
    %parsedParameters = %{ $defaultParametersRef };
    $defaultItemFactory = shift;

    %parsedItems = ();

    bless($reference, $object);
    return $reference;
}

sub compile()
{
    my $object = shift;

    my $inputFile = shift;
    my $generateCode = shift;

    my $file = new IO::File;
    open($file, $inputFile) or die "Failed to open file: $!";

    my $InParser = InFilesParser->new();
    $InParser->parse($file, \&parameterHandler, \&itemHandler);

    close($file);
    die "Failed to read from file: $inputFile" if (keys %parsedItems == 0);

    &$generateCode(\%parsedParameters, \%parsedItems);
}

sub license()
{
    return "/*
 * THIS FILE WAS AUTOMATICALLY GENERATED, DO NOT EDIT.
 *
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

";
}

1;
