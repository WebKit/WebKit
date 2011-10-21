#!/usr/bin/perl -w

# Copyright (C) 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
# Copyright (C) 2009, Julien Chaffraix <jchaffraix@webkit.org>
# Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
# Copyright (C) 2011 Ericsson AB. All rights reserved.
# Copyright (C) 2011 Google, Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use strict;

use Config;
use Getopt::Long;
use File::Path;
use File::Spec;
use IO::File;
use InFilesParser;

sub readEvents($);
sub printFactoryFile($);
sub printMacroFile($);
sub printHeadersFile($);

my $eventsFile = "";
my $outputDir = ".";
my %parsedEvents = ();

require Config;

GetOptions(
    'events=s' => \$eventsFile,
    'outputDir=s' => \$outputDir,
);

mkpath($outputDir);

die "You must specify --events <file>" unless length($eventsFile);

my %events = %{readEvents($eventsFile)};

printFactoryFile("$outputDir/EventFactory.cpp");
printMacroFile("$outputDir/EventInterfaces.h");
printHeadersFile("$outputDir/EventHeaders.h");

sub defaultEventPropertyHash
{
    return (
        'interfaceName' => 0,
        'conditional' => 0
    );
}

sub eventHandler($$$)
{
    my ($event, $property, $value) = @_;

    $parsedEvents{$event} = { defaultEventPropertyHash($event) } if !defined($parsedEvents{$event});

    if ($property) {
        die "Unknown property $property for event $event\n" if !defined($parsedEvents{$event}{$property});
        $parsedEvents{$event}{$property} = $value;
    }
}

sub parametersHandler
{
    my ($parameter, $value) = @_;

    die "Unknown parameter $parameter for events\n" unless $parameter eq "namespace";
}

sub readNames($$$)
{
    my ($namesFile, $hashToFillRef, $handler) = @_;

    my $names = new IO::File;
    open($names, $namesFile) or die "Failed to open file: $namesFile";

    my $InParser = InFilesParser->new();
    $InParser->parse($names, \&parametersHandler, $handler);

    close($names);
    die "Failed to read names from file: $namesFile" if (keys %{$hashToFillRef} == 0);
    return $hashToFillRef;
}

sub readEvents($)
{
    my ($namesFile) = @_;
    %parsedEvents = ();
    return readNames($namesFile, \%parsedEvents, \&eventHandler);
}

sub interfaceForEvent($)
{
    my ($eventName) = @_;

    my $interfaceName = $parsedEvents{$eventName}{"interfaceName"};
    $interfaceName = $eventName unless $interfaceName;

    return $interfaceName;
}

sub printFactoryFile($)
{
    my $path = shift;
    my $F;

    open F, ">$path";

    printLicenseHeader($F);

    print F "#include \"config.h\"\n";
    print F "#include \"EventFactory.h\"\n";
    print F "\n";
    print F "#include \"EventHeaders.h\"\n";
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    print F "PassRefPtr<Event> EventFactory::create(const String& eventType)\n";
    print F "{\n";

    for my $eventName (sort keys %parsedEvents) {
        my $conditional = $parsedEvents{$eventName}{"conditional"};
        my $interfaceName = interfaceForEvent($eventName);

        print F "#if ENABLE($conditional)\n" if $conditional;
        print F "    if (eventType == \"$eventName\")\n";
        print F "        return ${interfaceName}::create();\n";
        print F "#endif\n" if $conditional;
    }

    print F "    return 0;\n";
    print F "}\n";
    print F "\n";
    print F "} // namespace WebCore\n";
    close F;
}

sub printMacroFile($)
{
    my $path = shift;
    my $F;

    open F, ">$path";

    printLicenseHeader($F);

    print F "#ifndef EventInterfaces_h\n";
    print F "#define EventInterfaces_h\n";
    print F "\n";

    my %unconditionalInterfaces = ();
    my %interfacesByConditional = ();

    for my $eventName (sort keys %parsedEvents) {
        my $conditional = $parsedEvents{$eventName}{"conditional"};
        my $interfaceName = interfaceForEvent($eventName);

        if ($conditional) {
            if (!defined($interfacesByConditional{$conditional})) {
                $interfacesByConditional{$conditional} = ();
            }
            $interfacesByConditional{$conditional}{$interfaceName} = 1;
        } else {
            $unconditionalInterfaces{$interfaceName} = 1
        }
    }

    for my $conditional (sort keys %interfacesByConditional) {
        print F "#if ENABLE($conditional)\n";
        print F "#define DOM_EVENT_INTERFACES_FOR_EACH_$conditional(macro) \\\n";

        for my $interface (sort keys %{ $interfacesByConditional{$conditional} }) {
            next if defined($unconditionalInterfaces{$interface});
            print F "    macro($interface) \\\n";
        }

        print F "// End of DOM_EVENT_INTERFACES_FOR_EACH_$conditional\n";
        print F "#else\n";
        print F "#define DOM_EVENT_INTERFACES_FOR_EACH_$conditional(macro)\n";
        print F "#endif\n";
        print F "\n";
    }

    print F "#define DOM_EVENT_INTERFACES_FOR_EACH(macro) \\\n";
    print F "    \\\n";
    for my $interface (sort keys %unconditionalInterfaces) {
            print F "    macro($interface) \\\n";
    }
    print F "    \\\n";
    for my $conditional (sort keys %interfacesByConditional) {
        print F "    DOM_EVENT_INTERFACES_FOR_EACH_$conditional(macro) \\\n";
    }

    print F "\n";
    print F "#endif // EventInterfaces_h\n";

    close F;
}

sub printHeadersFile($)
{
    my $path = shift;
    my $F;

    open F, ">$path";

    printLicenseHeader($F);

    print F "#ifndef EventHeaders_h\n";
    print F "#define EventHeaders_h\n";
    print F "\n";

    my %includedInterfaces = ();

    for my $eventName (sort keys %parsedEvents) {
        my $conditional = $parsedEvents{$eventName}{"conditional"};
        my $interfaceName = interfaceForEvent($eventName);

        next if defined($includedInterfaces{$interfaceName});
        $includedInterfaces{$interfaceName} = 1;

        print F "#if ENABLE($conditional)\n" if $conditional;
        print F "#include \"$interfaceName.h\"\n";
        print F "#if USE(JSC)\n";
        print F "#include \"JS$interfaceName.h\"\n";
        print F "#elif USE(V8)\n";
        print F "#include \"V8$interfaceName.h\"\n";
        print F "#endif\n";
        print F "#endif\n" if $conditional;
    }

    print F "\n";
    print F "#endif // EventHeaders_h\n";

    close F;
}

sub printLicenseHeader($)
{
    my ($F) = @_;

    print F "/*
 * THIS FILE WAS AUTOMATICALLY GENERATED, DO NOT EDIT.
 *
 * This file was generated by the dom/make_events.pl script.
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

