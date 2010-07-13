# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

package CodeGeneratorInspector;

use strict;

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
    "push" => "push"
};
$typeTransform{"Array"} = {
    "param" => "PassRefPtr<InspectorArray>",
    "retVal" => "PassRefPtr<InspectorArray>",
    "forward" => "InspectorArray",
    "header" => "InspectorValues.h",
    "push" => "push"
};
$typeTransform{"Value"} = {
    "param" => "PassRefPtr<InspectorValue>",
    "retVal" => "PassRefPtr<InspectorValue>",
    "forward" => "InspectorValue",
    "header" => "InspectorValues.h",
    "push" => "push"
};
$typeTransform{"String"} = {
    "param" => "const String&",
    "retVal" => "String",
    "forward" => "String",
    "header" => "PlatformString.h",
    "push" => "pushString"
};
$typeTransform{"long"} = {
    "param" => "long",
    "retVal" => "long",
    "forward" => "",
    "header" => "",
    "push" => "pushNumber"
};
$typeTransform{"int"} = {
    "param" => "int",
    "retVal" => "int",
    "forward" => "",
    "header" => "",
    "push" => "pushNumber"
};
$typeTransform{"unsigned long"} = {
    "param" => "unsigned long",
    "retVal" => "unsigned long",
    "forward" => "",
    "header" => "",
    "push" => "pushNumber"
};
$typeTransform{"boolean"} = {
    "param" => "bool",
    "retVal"=> "bool",
    "forward" => "",
    "header" => "",
    "push" => "pushBool"
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
my %discoveredTypes;
my %generatedFunctions;

my @frontendClassDeclaration;
my @functionDefinitions;

sub typeSpec
{
    my $param = shift;
    my $retValue = shift;

    my $type = $typeTransform{$param->type}->{$retValue ? "retVal" : "param"};
    $discoveredTypes{$param->type} = 1;
    $type or die "invalid type specification \"" . $param->type ."\"";
    return $type;
}

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

    $discoveredTypes{"String"} = 1;
    $discoveredTypes{"InspectorClient"} = 1;
    $discoveredTypes{"PassRefPtr"} = 1;

    push(@frontendClassDeclaration, "class $frontendClassName {");
    push(@frontendClassDeclaration, "public:");
    push(@frontendClassDeclaration, "    $frontendClassName(InspectorClient* inspectorClient) : m_inspectorClient(inspectorClient) { }");
    push(@frontendClassDeclaration, "");
    push(@frontendClassDeclaration, generateFunctions($interface, $frontendClassName));
    push(@frontendClassDeclaration, "");
    push(@frontendClassDeclaration, "private:");
    push(@frontendClassDeclaration, "    InspectorClient* m_inspectorClient;");
    push(@frontendClassDeclaration, "};");
}

sub generateFunctions
{
    my $interface = shift;
    my $className = shift;

    my @functionDeclarations;
    foreach my $function (@{$interface->functions}) {
        my $functionName;
        my $argumentsDirectionFilter;
        my $arguments;
        my @functionBody;
        push(@functionBody, "    RefPtr<InspectorArray> arguments = InspectorArray::create();");
        if ($function->signature->extendedAttributes->{"notify"}) {
            $functionName = $function->signature->name;
            $argumentsDirectionFilter = "in";
            $arguments = "";
            push(@functionBody, "    arguments->pushString(\"$functionName\");");
        } else {
            my $customResponse = $function->signature->extendedAttributes->{"customResponse"};
            $functionName = $customResponse ? $customResponse : "did" . ucfirst($function->signature->name);
            $argumentsDirectionFilter = "out";
            $arguments = "long callId";
            push(@functionBody, "    arguments->pushString(\"$functionName\");");
            push(@functionBody, "    arguments->pushNumber(callId);");
        }

        foreach my $parameter (@{$function->parameters}) {
            if ($parameter->direction eq $argumentsDirectionFilter) {
                $parameter->name or die "empty argument name specified for function ${frontendClassName}::$functionName and argument type " . $parameter->type;
                $arguments = $arguments . ", " if ($arguments);
                $arguments = $arguments . typeSpec($parameter) . " " . $parameter->name;
                my $pushFunctionName =  $typeTransform{$parameter->type}->{"push"};
                push(@functionBody, "    arguments->$pushFunctionName(" . $parameter->name . ");");
            }
        }
        push(@functionBody, "    m_inspectorClient->sendMessageToFrontend(arguments->toJSONString());");

        my $signature = "    " . typeSpec($function->signature, 1) . " $functionName($arguments);";
        if (!$generatedFunctions{${signature}}) {
            $generatedFunctions{${signature}} = 1;
            push(@functionDeclarations, $signature);

            my @function;
            push(@function, typeSpec($function->signature, 1) . " " . $className . "::" . $functionName . "(" . $arguments . ")");
            push(@function, "{");
            push(@function, @functionBody);
            push(@function, "}");
            push(@function, "");
            push(@functionDefinitions, @function);
        }
    }
    return @functionDeclarations;
}

sub generateHeader
{
    my @headerContent = split("\r", $licenseTemplate);
    push(@headerContent, "#ifndef ${frontendClassName}_h");
    push(@headerContent, "#define ${frontendClassName}_h");
    push(@headerContent, "");

    my @forwardHeaders;
    foreach my $type (keys %discoveredTypes) {
        push(@forwardHeaders, "#include <" . $typeTransform{$type}->{"forwardHeader"} . ">") if !$typeTransform{$type}->{"forwardHeader"} eq  "";
    }
    push(@headerContent, sort @forwardHeaders);
    push(@headerContent, "");
    push(@headerContent, "namespace $namespace {");
    push(@headerContent, "");

    my @forwardDeclarations;
    foreach my $type (keys %discoveredTypes) {
        push(@forwardDeclarations, "class " . $typeTransform{$type}->{"forward"} . ";") if !$typeTransform{$type}->{"forward"} eq  "";
    }
    push(@headerContent, sort @forwardDeclarations);

    push(@headerContent, "");
    push(@headerContent, @frontendClassDeclaration);
    push(@headerContent, "");
    push(@headerContent, "} // namespace $namespace");
    push(@headerContent, "");
    push(@headerContent, "#endif // !defined(${frontendClassName}_h)");
    push(@headerContent, "");
    return @headerContent;
}

sub generateSource
{
    my @sourceContent = split("\r", $licenseTemplate);
    push(@sourceContent, "\n#include \"config.h\"");
    push(@sourceContent, "#include \"$frontendClassName.h\"");
    push(@sourceContent, "");
    push(@sourceContent, "#if ENABLE(INSPECTOR)");
    push(@sourceContent, "");

    my %headers;
    foreach my $type (keys %discoveredTypes) {
        $headers{"#include \"" . $typeTransform{$type}->{"header"} . "\""} = 1 if !$typeTransform{$type}->{"header"} eq  "";
    }
    push(@sourceContent, sort keys %headers);
    push(@sourceContent, "");
    push(@sourceContent, "namespace $namespace {");
    push(@sourceContent, "");
    push(@sourceContent, @functionDefinitions);
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
    open(my $HEADER, ">$outputDir/$frontendClassName.h") || die "Couldn't open file $outputDir/$frontendClassName.h";

    print $SOURCE join("\n", generateSource());
    close($SOURCE);
    undef($SOURCE);

    print $HEADER join("\n", generateHeader());
    close($HEADER);
    undef($HEADER);
}

1;
