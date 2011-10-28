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
use InFilesCompiler;

require Config;

my $inputFile = "";
my $outputDir = ".";

GetOptions(
    'input=s' => \$inputFile,
    'outputDir=s' => \$outputDir,
);

mkpath($outputDir);

die "You must specify --input <file>" unless length($inputFile);

my %defaultParameters = (
    'namespace' => 0
);

my $InCompiler = InFilesCompiler->new(\%defaultParameters, \&defaultItemFactory);

$InCompiler->compile($inputFile, \&generateCode);

sub defaultItemFactory
{
    return (
        'interfaceName' => 0,
        'conditional' => 0
    );
}

sub interfaceForEvent($$)
{
    my ($eventName, $parsedItemsRef) = @_;

    my %parsedItems = %{ $parsedItemsRef };

    my $interfaceName = $parsedItems{$eventName}{"interfaceName"};
    $interfaceName = $eventName unless $interfaceName;

    return $interfaceName;
}

sub toMacroStyle($)
{
    my ($camelCase) = @_;

    return "EVENT" if $camelCase eq "Event";
    return "EVENT_TARGET" if $camelCase eq "EventTarget";

    die "Ok, you got me. This script is really just a giant hack. (\$camelCase=${camelCase})";
}

sub generateCode()
{
    my $parsedParametersRef = shift;
    my $parsedItemsRef = shift;

    printFactoryFile($parsedParametersRef, $parsedItemsRef);
    printMacroFile($parsedParametersRef, $parsedItemsRef);
    printHeadersFile($parsedParametersRef, $parsedItemsRef);
}

sub printFactoryFile()
{
    my $parsedParametersRef = shift;
    my $parsedItemsRef = shift;

    my $F;
    my %parsedEvents = %{ $parsedItemsRef };
    my %parsedParameters = %{ $parsedParametersRef };

    my $namespace = $parsedParameters{"namespace"};

    # Currently, only Events have factory files.
    return if $namespace ne "Event";

    my $outputFile = "$outputDir/${namespace}Factory.cpp";

    open F, ">$outputFile" or die "Failed to open file: $!";

    print F $InCompiler->license();

    print F "#include \"config.h\"\n";
    print F "#include \"${namespace}Factory.h\"\n";
    print F "\n";
    print F "#include \"${namespace}Headers.h\"\n";
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    print F "PassRefPtr<$namespace> ${namespace}Factory::create(const String& type)\n";
    print F "{\n";

    for my $eventName (sort keys %parsedEvents) {
        my $conditional = $parsedEvents{$eventName}{"conditional"};
        my $interfaceName = interfaceForEvent($eventName, $parsedItemsRef);

        print F "#if ENABLE($conditional)\n" if $conditional;
        print F "    if (type == \"$eventName\")\n";
        print F "        return ${interfaceName}::create();\n";
        print F "#endif\n" if $conditional;
    }

    print F "    return 0;\n";
    print F "}\n";
    print F "\n";
    print F "} // namespace WebCore\n";

    close F;
}

sub printMacroFile()
{
    my $parsedParametersRef = shift;
    my $parsedItemsRef = shift;

    my $F;
    my %parsedEvents = %{ $parsedItemsRef };
    my %parsedParameters = %{ $parsedParametersRef };

    my $namespace = $parsedParameters{"namespace"};

    my $outputFile = "$outputDir/${namespace}Interfaces.h";

    open F, ">$outputFile" or die "Failed to open file: $!";

    print F $InCompiler->license();

    print F "#ifndef ${namespace}Interfaces_h\n";
    print F "#define ${namespace}Interfaces_h\n";
    print F "\n";

    my %unconditionalInterfaces = ();
    my %interfacesByConditional = ();

    for my $eventName (sort keys %parsedEvents) {
        my $conditional = $parsedEvents{$eventName}{"conditional"};
        my $interfaceName = interfaceForEvent($eventName, $parsedItemsRef);

        if ($conditional) {
            if (!defined($interfacesByConditional{$conditional})) {
                $interfacesByConditional{$conditional} = ();
            }
            $interfacesByConditional{$conditional}{$interfaceName} = 1;
        } else {
            $unconditionalInterfaces{$interfaceName} = 1
        }
    }

    my $macroStyledNamespace = toMacroStyle($namespace);

    for my $conditional (sort keys %interfacesByConditional) {
        print F "#if ENABLE($conditional)\n";
        print F "#define DOM_${macroStyledNamespace}_INTERFACES_FOR_EACH_$conditional(macro) \\\n";

        for my $interface (sort keys %{ $interfacesByConditional{$conditional} }) {
            next if defined($unconditionalInterfaces{$interface});
            print F "    macro($interface) \\\n";
        }

        print F "// End of DOM_${macroStyledNamespace}_INTERFACES_FOR_EACH_$conditional\n";
        print F "#else\n";
        print F "#define DOM_${macroStyledNamespace}_INTERFACES_FOR_EACH_$conditional(macro)\n";
        print F "#endif\n";
        print F "\n";
    }

    print F "#define DOM_${macroStyledNamespace}_INTERFACES_FOR_EACH(macro) \\\n";
    print F "    \\\n";
    for my $interface (sort keys %unconditionalInterfaces) {
            print F "    macro($interface) \\\n";
    }
    print F "    \\\n";
    for my $conditional (sort keys %interfacesByConditional) {
        print F "    DOM_${macroStyledNamespace}_INTERFACES_FOR_EACH_$conditional(macro) \\\n";
    }

    print F "\n";
    print F "#endif // ${namespace}Interfaces_h\n";

    close F;
}

sub printHeadersFile()
{
    my $parsedParametersRef = shift;
    my $parsedItemsRef = shift;

    my $F;
    my %parsedEvents = %{ $parsedItemsRef };
    my %parsedParameters = %{ $parsedParametersRef };

    my $namespace = $parsedParameters{"namespace"};

    my $outputFile = "$outputDir/${namespace}Headers.h";

    open F, ">$outputFile" or die "Failed to open file: $!";

    print F $InCompiler->license();

    print F "#ifndef ${namespace}Headers_h\n";
    print F "#define ${namespace}Headers_h\n";
    print F "\n";

    my %includedInterfaces = ();

    for my $eventName (sort keys %parsedEvents) {
        my $conditional = $parsedEvents{$eventName}{"conditional"};
        my $interfaceName = interfaceForEvent($eventName, $parsedItemsRef);

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
    print F "#endif // ${namespace}Headers_h\n";

    close F;
}
