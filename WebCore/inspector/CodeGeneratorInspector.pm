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
    "param" => "const RefPtr<InspectorValue>&",
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
my $fileName;
my %discoveredTypes;

my @classDefinition;
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
}

# Params: 'idlDocument' struct
sub GenerateInterface
{
    my $object = shift;
    my $interface = shift;
    my $defines = shift;

    my $className = $interface->name;
    $fileName = $className;

    $discoveredTypes{"String"} = 1;
    $discoveredTypes{"InspectorClient"} = 1;
    $discoveredTypes{"PassRefPtr"} = 1;

    push(@classDefinition, "class $className {");
    push(@classDefinition, "public:");
    push(@classDefinition, "    $className(InspectorClient* inspectorClient) : m_inspectorClient(inspectorClient) { }");
    push(@classDefinition, "");
    push(@classDefinition, generateFunctionsDeclarations($interface, $className));
    push(@classDefinition, "");
    push(@classDefinition, "private:");
    push(@classDefinition, "    void sendSimpleMessageToFrontend(const String&);");
    push(@classDefinition, "    InspectorClient* m_inspectorClient;");
    push(@classDefinition, "};");

    push(@functionDefinitions, "void ${className}::sendSimpleMessageToFrontend(const String& functionName)");
    push(@functionDefinitions, "{");
    push(@functionDefinitions, "    RefPtr<InspectorArray> arguments = InspectorArray::create();");
    push(@functionDefinitions, "    arguments->pushString(functionName);");
    push(@functionDefinitions, "    m_inspectorClient->sendMessageToFrontend(arguments->toJSONString());");
    push(@functionDefinitions, "}");
}

sub generateFunctionsDeclarations
{
    my $interface = shift;
    my $className = shift;

    my @functionDeclarations;
    foreach my $function (@{$interface->functions}) {
        my $functionName = $function->signature->name;
        my $abstract = $function->signature->extendedAttributes->{"abstract"};
        my $arguments = "";
        foreach my $parameter (@{$function->parameters}) {
            $parameter->name or die "empty argument name specified for function ${className}::$functionName and argument type " . $parameter->type;
            $arguments = $arguments . ", " if ($arguments);
            $arguments = $arguments . typeSpec($parameter) . " " . $parameter->name;
        }
        my $signature = "    " . typeSpec($function->signature, 1) . " $functionName($arguments)";
        push(@functionDeclarations, $abstract ? "$signature = 0;" : "$signature;");
        push(@functionDefinitions, generateFunctionsImpl($className, $function, $arguments)) if !$abstract;
    }
    return @functionDeclarations;
}

sub generateHeader
{
    my @headerContent = split("\r", $licenseTemplate);
    push(@headerContent, "#ifndef ${fileName}_h");
    push(@headerContent, "#define ${fileName}_h");
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
    push(@headerContent, @classDefinition);
    push(@headerContent, "");
    push(@headerContent, "} // namespace $namespace");
    push(@headerContent, "");
    push(@headerContent, "#endif // !defined(${fileName}_h)");
    push(@headerContent, "");
    return @headerContent;
}

sub generateSource
{
    my @sourceContent = split("\r", $licenseTemplate);
    push(@sourceContent, "\n#include \"config.h\"");
    push(@sourceContent, "#include \"Remote$fileName.h\"");
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

sub generateFunctionsImpl
{
    my $className = shift;
    my $function = shift;
    my $arguments = shift;

    my @func;

    my $functionName = $function->signature->name;

    push(@func, typeSpec($function->signature, 1) . " " . $className . "::" . $functionName . "(" . $arguments . ")");
    push(@func, "{");

    my $numParameters = @{$function->parameters};
    if ($numParameters > 0) {
        push(@func, "    RefPtr<InspectorArray> arguments = InspectorArray::create();");
        push(@func, "    arguments->pushString(\"$functionName\");");
        foreach my $parameter (@{$function->parameters}) {
            my $pushCall =  $typeTransform{$parameter->type}->{"push"};
            push(@func, "    arguments->$pushCall(" . $parameter->name . ");");
        }
        push(@func, "    m_inspectorClient->sendMessageToFrontend(arguments->toJSONString());");
    } else {
        push(@func, "    sendSimpleMessageToFrontend(\"$functionName\");");
    }

    push(@func, "}");
    push(@func, "");
    return @func;
}

sub finish
{
    my $object = shift;

    open(my $SOURCE, ">$outputDir/Remote$fileName.cpp") || die "Couldn't open file $outputDir/Remote$fileName.cpp";
    open(my $HEADER, ">$outputDir/Remote$fileName.h") || die "Couldn't open file $outputDir/Remote$fileName.h";

    print $SOURCE join("\n", generateSource());
    close($SOURCE);
    undef($SOURCE);

    print $HEADER join("\n", generateHeader());
    close($HEADER);
    undef($HEADER);
}

1;
