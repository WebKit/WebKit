#!/usr/bin/env perl

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
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
use warnings;
use FindBin;
use lib "$FindBin::Bin/../bindings/scripts";

use InFilesCompiler;

my %defaultParameters = (
    'namespace' => 0,
    'factoryFunction' => 0,
    'useNamespaceAsSuffix' => 0,
);

sub defaultItemFactory
{
    return (
        'interfaceName' => 0,
        'conditional' => 0,
        'runtimeEnabled' => 0
    );
}

my $InCompiler = InFilesCompiler->new(\%defaultParameters, \&defaultItemFactory);

my $outputDir = $InCompiler->initializeFromCommandLine();
$InCompiler->compile(\&generateCode);

sub generateCode()
{
    my $parsedParametersRef = shift;
    my $parsedItemsRef = shift;

    generateImplementation($parsedParametersRef, $parsedItemsRef);
    $InCompiler->generateInterfacesHeader();
    $InCompiler->generateHeadersHeader();
}

sub generateImplementation()
{
    my $parsedParametersRef = shift;
    my $parsedItemsRef = shift;

    my %parsedEvents = %{ $parsedItemsRef };
    my %parsedParameters = %{ $parsedParametersRef };

    my $namespace = $parsedParameters{"namespace"};
    my $factoryFunction = $parsedParameters{"factoryFunction"};
    ($factoryFunction eq "toJS" or $factoryFunction eq "toNewlyCreated") or die "factoryFunction should be either toJS or toNewlyCreated";
    my $useNamespaceAsSuffix = $parsedParameters{"useNamespaceAsSuffix"};

    my $F;
    open F, ">", "$outputDir/${namespace}Factory.cpp" or die "Failed to open file: $!";

    print F $InCompiler->license();

    my $interfaceMethodName = lcfirst $namespace . "Interface";

    print F "#include \"config.h\"\n";
    print F "#include \"${namespace}Headers.h\"\n";
    print F "\n";
    print F "#include \"JSDOMGlobalObject.h\"\n";
    print F "#include <JavaScriptCore/StructureInlines.h>\n";
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    # FIXME: Why does Event need toNewlyCreated but EventTarget need toJS?
    if ($factoryFunction eq "toNewlyCreated") {
        print F "JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject* globalObject, Ref<${namespace}>&& impl)\n";
        print F "{\n";
        print F "    switch (impl->${interfaceMethodName}()) {\n";
    } else {
        print F "JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, ${namespace}& impl)\n";
        print F "{\n";
        print F "    switch (impl.${interfaceMethodName}()) {\n";
    }

    my %generatedInterfaceNames = ();

    for my $eventName (sort keys %parsedEvents) {
        my $conditional = $parsedEvents{$eventName}{"conditional"};
        my $runtimeEnabled = $parsedEvents{$eventName}{"runtimeEnabled"};
        my $interfaceName = $InCompiler->interfaceForItem($eventName);

        next if $generatedInterfaceNames{$interfaceName};
        $generatedInterfaceNames{$interfaceName} = 1;

        my $suffix = "";
        if ($useNamespaceAsSuffix eq "true") {
            $suffix = $namespace . $suffix;
        }

        # FIXME: This should pay attention to $runtimeConditional so it can support RuntimeEnabledFeatures.
        if ($conditional) {
            my $conditionals = "#if ENABLE(" . join(") || ENABLE(", split("\\|", $conditional)) . ")";
            print F "$conditionals\n";
        }
        print F "    case ${interfaceName}${suffix}InterfaceType:\n";
        if ($factoryFunction eq "toNewlyCreated") {
            print F "        return createWrapper<$interfaceName$suffix>(globalObject, WTFMove(impl));\n";
        } else {
            print F "        return toJS(state, globalObject, static_cast<$interfaceName&>(impl));\n";
        }
        print F "#endif\n" if $conditional;
    }

    print F "    }\n";
    if ($factoryFunction eq "toNewlyCreated") {
        print F "    return createWrapper<$namespace>(globalObject, WTFMove(impl));\n";
    } else {
        print F "    ASSERT_NOT_REACHED();\n";
        print F "    return JSC::jsNull();\n";
    }
    print F "}\n";
    print F "\n";
    print F "} // namespace WebCore\n";

    close F;
}
