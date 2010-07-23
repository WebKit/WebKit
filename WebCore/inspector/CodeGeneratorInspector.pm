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
$typeTransform{"InspectorBackend"} = {
    "forward" => "InspectorBackend",
    "header" => "InspectorBackend.h",
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
my $outputHeadersDir;
my $writeDependencies;
my $verbose;

my $namespace;

my $backendClassName;
my %backendTypes;
my %backendMethods;
my @backendMethodsImpl;
my $backendConstructor;
my $backendFooter;

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
    $outputHeadersDir = shift;
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

    $backendClassName = $className . "BackendDispatcher";
    my @backendHead;
    push(@backendHead, "    ${backendClassName}(InspectorBackend* inspectorBackend) : m_inspectorBackend(inspectorBackend) { }");
    push(@backendHead, "    void dispatch(const String& message, String* exception);");
    push(@backendHead, "private:");
    $backendConstructor = join("\n", @backendHead);
    $backendFooter = "    InspectorBackend* m_inspectorBackend;";
    $backendTypes{"InspectorBackend"} = 1;
    $backendTypes{"PassRefPtr"} = 1;
    $backendTypes{"Array"} = 1;

    generateBackendPrivateFunctions();
    generateFunctions($interface);
}

sub generateFunctions
{
    my $interface = shift;

    foreach my $function (@{$interface->functions}) {
        generateFrontendFunction($function);
        generateBackendFunction($function);
    }
    push(@backendMethodsImpl, generateBackendDispatcher());
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

sub generateBackendPrivateFunctions
{
    my $privateFunctions = << "EOF";
static String formatWrongArgumentsCountMessage(unsigned expected, unsigned actual)
{
    return String::format(\"Wrong number of parameters: %d (expected: %d)\", actual, expected);
}

static String formatWrongArgumentTypeMessage(unsigned position, const char* name, const char* expectedType)
{
    return String::format(\"Failed to convert parameter %d (%s) to %s\", position, name, expectedType);
}
EOF
    push(@backendMethodsImpl, $privateFunctions);
}

sub generateBackendFunction
{
    my $function = shift;
    return if $function->signature->extendedAttributes->{"notify"};

    my $functionName = $function->signature->name;

    my @argsFiltered = grep($_->direction eq "in", @{$function->parameters});
    map($backendTypes{$_->type} = 1, @argsFiltered); # register required types
    my $arguments = join(", ", map($typeTransform{$_->type}->{"param"} . " " . $_->name, @argsFiltered));

    my $signature = "    void ${functionName}(PassRefPtr<InspectorArray> args, String* exception);";
    !$backendMethods{${signature}} || die "Duplicate function was detected for signature '$signature'.";
    $backendMethods{${signature}} = $functionName;

    my @function;
    push(@function, "void ${backendClassName}::${functionName}(PassRefPtr<InspectorArray> args, String* exception)");
    push(@function, "{");
    my $i = 1; # zero element is the method name.
    my $expectedParametersCount = scalar(@argsFiltered);
    push(@function, "    if (args->length() != $expectedParametersCount) {");
    push(@function, "        *exception = formatWrongArgumentsCountMessage(args->length(), $expectedParametersCount);");
    push(@function, "        ASSERT_NOT_REACHED();");
    push(@function, "        return;");
    push(@function, "    }");

    foreach my $parameter (@argsFiltered) {
        my $parameterType = $parameter->type;
        push(@function, "    " . $typeTransform{$parameterType}->{"retVal"} . " " . $parameter->name . ";");
        push(@function, "    if (!args->get(" . $i . ")->as" . $typeTransform{$parameterType}->{"accessorSuffix"} . "(&" . $parameter->name . ")) {");
        push(@function, "        *exception = formatWrongArgumentTypeMessage($i, \"" . $parameter->name . "\", \"$parameterType\");");
        push(@function, "        ASSERT_NOT_REACHED();");
        push(@function, "        return;");
        push(@function, "    }");
        ++$i;
    }
    push(@function, "    m_inspectorBackend->$functionName(" . join(", ", map($_->name, @argsFiltered)) . ");");
    push(@function, "}");
    push(@function, "");
    push(@backendMethodsImpl, @function);
}

sub generateBackendDispatcher
{
    my @body;
    my @methods = map($backendMethods{$_}, keys %backendMethods);
    my @mapEntries = map("dispatchMap.add(\"$_\", &${backendClassName}::$_);", @methods);

    push(@body, "void ${backendClassName}::dispatch(const String& message, String* exception)");
    push(@body, "{");
    push(@body, "    typedef void (${backendClassName}::*CallHandler)(PassRefPtr<InspectorArray> args, String* exception);");
    push(@body, "    typedef HashMap<String, CallHandler> DispatchMap;");
    push(@body, "    DEFINE_STATIC_LOCAL(DispatchMap, dispatchMap, );");
    push(@body, "    if (dispatchMap.isEmpty()) {");
    push(@body, map("        $_", @mapEntries));
    push(@body, "    }");
    push(@body, "");
    push(@body, "    RefPtr<InspectorValue> parsedMessage = InspectorValue::parseJSON(message);");
    push(@body, "    if (!parsedMessage) {");
    push(@body, "        ASSERT_NOT_REACHED();");
    push(@body, "        *exception = \"Error: Invalid message format. Message should be in JSON format.\";");
    push(@body, "        return;");
    push(@body, "    }");
    push(@body, "");
    push(@body, "    RefPtr<InspectorArray> messageArray = parsedMessage->asArray();");
    push(@body, "    if (!messageArray) {");
    push(@body, "        ASSERT_NOT_REACHED();");
    push(@body, "        *exception = \"Error: Invalid message format. The message should be a JSONified array of arguments.\";");
    push(@body, "        return;");
    push(@body, "    }");
    push(@body, "");
    push(@body, "    if (!messageArray->length()) {");
    push(@body, "        ASSERT_NOT_REACHED();");
    push(@body, "        *exception = \"Error: Invalid message format. Empty message was received.\";");
    push(@body, "        return;");
    push(@body, "    }");
    push(@body, "");
    push(@body, "    String methodName;");
    push(@body, "    if (!messageArray->get(0)->asString(&methodName)) {");
    push(@body, "        ASSERT_NOT_REACHED();");
    push(@body, "        *exception = \"Error: Invalid message format. The first element of the message should be method name.\";");
    push(@body, "        return;");
    push(@body, "    }");
    push(@body, "");
    push(@body, "    HashMap<String, CallHandler>::iterator it = dispatchMap.find(methodName);");
    push(@body, "    if (it == dispatchMap.end()) {");
    push(@body, "        ASSERT_NOT_REACHED();");
    push(@body, "        *exception = String::format(\"Error: Invalid method name. '%s' wasn't found.\", methodName.utf8().data());");
    push(@body, "        return;");
    push(@body, "    }");
    push(@body, "");
    push(@body, "    ((*this).*it->second)(messageArray, exception);");
    push(@body, "}");
    return @body;
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

    open(my $HEADER, ">$outputHeadersDir/$frontendClassName.h") || die "Couldn't open file $outputHeadersDir/$frontendClassName.h";
    print $HEADER generateHeader($frontendClassName, \%frontendTypes, $frontendConstructor, \%frontendMethods, $frontendFooter);
    close($HEADER);
    undef($HEADER);

    open($SOURCE, ">$outputDir/$backendClassName.cpp") || die "Couldn't open file $outputDir/$backendClassName.cpp";
    print $SOURCE join("\n", generateSource($backendClassName, \%backendTypes, \@backendMethodsImpl));
    close($SOURCE);
    undef($SOURCE);

    open($HEADER, ">$outputHeadersDir/$backendClassName.h") || die "Couldn't open file $outputHeadersDir/$backendClassName.h";
    print $HEADER join("\n", generateHeader($backendClassName, \%backendTypes, $backendConstructor, \%backendMethods, $backendFooter));
    close($HEADER);
    undef($HEADER);
}

1;
