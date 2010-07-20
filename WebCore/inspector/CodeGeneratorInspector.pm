# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

package CodeGeneratorInspector;

use strict;

use Class::Struct;
use File::stat;

my %typeTransform;
$typeTransform{"InspectorClient"} = {
    "forward" => "InspectorClient",
    "header" => "InspectorClient.h",
};
$typeTransform{"PassRefPtr"} = {
    "forwardHeader" => "wtf/PassRefPtr.h",
};
$typeTransform{"Object"} = {
    "param" => "PassRefPtr<InspectorObject>",
    "retVal" => "PassRefPtr<InspectorObject>",
    "forward" => "InspectorObject",
    "header" => "InspectorValues.h",
    "accessorSuffix" => ""
};
$typeTransform{"Array"} = {
    "param" => "PassRefPtr<InspectorArray>",
    "retVal" => "PassRefPtr<InspectorArray>",
    "forward" => "InspectorArray",
    "header" => "InspectorValues.h",
    "accessorSuffix" => ""
};
$typeTransform{"Value"} = {
    "param" => "PassRefPtr<InspectorValue>",
    "retVal" => "PassRefPtr<InspectorValue>",
    "forward" => "InspectorValue",
    "header" => "InspectorValues.h",
    "accessorSuffix" => ""
};
$typeTransform{"String"} = {
    "param" => "const String&",
    "retVal" => "String",
    "forward" => "String",
    "header" => "PlatformString.h",
    "accessorSuffix" => "String"
};
$typeTransform{"long"} = {
    "param" => "long",
    "retVal" => "long",
    "forward" => "",
    "header" => "",
    "accessorSuffix" => "Number"
};
$typeTransform{"int"} = {
    "param" => "int",
    "retVal" => "int",
    "forward" => "",
    "header" => "",
    "accessorSuffix" => "Number",
};
$typeTransform{"unsigned long"} = {
    "param" => "unsigned long",
    "retVal" => "unsigned long",
    "forward" => "",
    "header" => "",
    "accessorSuffix" => "Number"
};
$typeTransform{"boolean"} = {
    "param" => "bool",
    "retVal"=> "bool",
    "forward" => "",
    "header" => "",
    "accessorSuffix" => "Bool"
};
$typeTransform{"void"} = {
    "retVal" => "void",
    "forward" => "",
    "header" => ""
};

# Default License Templates

my $licenseTemplate = << "EOF";
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
EOF

my $codeGenerator;
my $outputDir;
my $writeDependencies;
my $verbose;

my $namespace;

my $frontendClassName;
my %frontendTypes;
my %frontendMethods;
my @frontendMethodsImpl;
my $frontendConstructor;
my $frontendFooter;

my $callId = new domSignature(); # it is just structure for describing parameters from IDLStructure.pm.
$callId->type("long");
$callId->name("callId");

# Default constructor
sub new
{
    my $object = shift;
    my $reference = { };

    $codeGenerator = shift;
    $outputDir = shift;
    shift; # $useLayerOnTop
    shift; # $preprocessor
    $writeDependencies = shift;
    $verbose = shift;

    bless($reference, $object);
    return $reference;
}

# Params: 'idlDocument' struct
sub GenerateModule
{
    my $object = shift;
    my $dataNode = shift;

    $namespace = $dataNode->module;
    $namespace =~ s/core/WebCore/;
}

# Params: 'idlDocument' struct
sub GenerateInterface
{
    my $object = shift;
    my $interface = shift;
    my $defines = shift;

    my $className = $interface->name;

    $frontendClassName = "Remote" . $className . "Frontend";
    $frontendConstructor = "    ${frontendClassName}(InspectorClient* inspectorClient) : m_inspectorClient(inspectorClient) { }";
    $frontendFooter = "    InspectorClient* m_inspectorClient;";
    $frontendTypes{"String"} = 1;
    $frontendTypes{"InspectorClient"} = 1;
    $frontendTypes{"PassRefPtr"} = 1;

    generateFunctions($interface);
}

sub generateFunctions
{
    my $interface = shift;

    foreach my $function (@{$interface->functions}) {
        generateFrontendFunction($function);
    }
}

sub generateFrontendFunction
{
    my $function = shift;

    my $functionName;
    my $notify = $function->signature->extendedAttributes->{"notify"};
    if ($notify) {
        $functionName = $function->signature->name;
    } else {
        my $customResponse = $function->signature->extendedAttributes->{"customResponse"};
        $functionName = $customResponse ? $customResponse : "did" . ucfirst($function->signature->name);
    }

    my @argsFiltered = grep($_->direction eq "out", @{$function->parameters}); # just keep only out parameters for frontend interface.
    unshift(@argsFiltered, $callId) if !$notify; # Add callId as the first argument for all frontend did* methods.
    map($frontendTypes{$_->type} = 1, @argsFiltered); # register required types.
    my $arguments = join(", ", map($typeTransform{$_->type}->{"param"} . " " . $_->name, @argsFiltered)); # prepare arguments for function signature.
    my @pushArguments = map("    arguments->push" . $typeTransform{$_->type}->{"accessorSuffix"} . "(" . $_->name . ");", @argsFiltered);

    my $signature = "    void ${functionName}(${arguments});";
    if (!$frontendMethods{${signature}}) {
        $frontendMethods{${signature}} = 1;

        my @function;
        push(@function, "void ${frontendClassName}::${functionName}(${arguments})");
        push(@function, "{");
        push(@function, "    RefPtr<InspectorArray> arguments = InspectorArray::create();");
        push(@function, "    arguments->pushString(\"$functionName\");");
        push(@function, @pushArguments);
        push(@function, "    m_inspectorClient->sendMessageToFrontend(arguments->toJSONString());");
        push(@function, "}");
        push(@function, "");
        push(@frontendMethodsImpl, @function);
    }
}

sub generateHeader
{
    my $className = shift;
    my $types = shift;
    my $constructor = shift;
    my $methods = shift;
    my $footer = shift;

    my $forwardHeaders = join("\n", sort(map("#include <" . $typeTransform{$_}->{"forwardHeader"} . ">", grep($typeTransform{$_}->{"forwardHeader"}, keys %{$types}))));
    my $forwardDeclarations = join("\n", sort(map("class " . $typeTransform{$_}->{"forward"} . ";", grep($typeTransform{$_}->{"forward"}, keys %{$types}))));
    my $methodsDeclarations = join("\n", keys %{$methods});

    my $headerBody = << "EOF";
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef ${className}_h
#define ${className}_h

${forwardHeaders}

namespace $namespace {

$forwardDeclarations

class $className {
public:
$constructor

$methodsDeclarations

private:
$footer
};

} // namespace $namespace
#endif // !defined(${className}_h)

EOF
    return $headerBody;
}

sub generateSource
{
    my $className = shift;
    my $types = shift;
    my $methods = shift;

    my @sourceContent = split("\r", $licenseTemplate);
    push(@sourceContent, "\n#include \"config.h\"");
    push(@sourceContent, "#include \"$className.h\"");
    push(@sourceContent, "");
    push(@sourceContent, "#if ENABLE(INSPECTOR)");
    push(@sourceContent, "");

    my %headers;
    foreach my $type (keys %{$types}) {
        $headers{"#include \"" . $typeTransform{$type}->{"header"} . "\""} = 1 if !$typeTransform{$type}->{"header"} eq  "";
    }
    push(@sourceContent, sort keys %headers);
    push(@sourceContent, "");
    push(@sourceContent, "namespace $namespace {");
    push(@sourceContent, "");
    push(@sourceContent, @{$methods});
    push(@sourceContent, "");
    push(@sourceContent, "} // namespace $namespace");
    push(@sourceContent, "");
    push(@sourceContent, "#endif // ENABLE(INSPECTOR)");
    push(@sourceContent, "");
    return @sourceContent;
}

sub finish
{
    my $object = shift;

    open(my $SOURCE, ">$outputDir/$frontendClassName.cpp") || die "Couldn't open file $outputDir/$frontendClassName.cpp";
    print $SOURCE join("\n", generateSource($frontendClassName, \%frontendTypes, \@frontendMethodsImpl));
    close($SOURCE);
    undef($SOURCE);

    open(my $HEADER, ">$outputDir/$frontendClassName.h") || die "Couldn't open file $outputDir/$frontendClassName.h";
    print $HEADER generateHeader($frontendClassName, \%frontendTypes, $frontendConstructor, \%frontendMethods, $frontendFooter);
    close($HEADER);
    undef($HEADER);

}

1;
