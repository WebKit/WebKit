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
$typeTransform{"Backend"} = {
    "forward" => "InspectorBackend",
    "header" => "InspectorBackend.h",
    "handlerAccessor" => "m_inspectorController->inspectorBackend()",
};
$typeTransform{"Controller"} = {
    "forwardHeader" => "InspectorController.h",
    "handlerAccessor" => "m_inspectorController",
};
$typeTransform{"Debug"} = {
    "forward" => "InspectorDebuggerAgent",
    "header" => "InspectorDebuggerAgent.h",
    "handlerAccessor" => "m_inspectorController->debuggerAgent()",
};
$typeTransform{"DOM"} = {
    "forward" => "InspectorDOMAgent",
    "header" => "InspectorDOMAgent.h",
    "handlerAccessor" => "m_inspectorController->domAgent()",
};
$typeTransform{"ApplicationCache"} = {
    "forward" => "InspectorApplicationCacheAgent",
    "header" => "InspectorApplicationCacheAgent.h",
    "handlerAccessor" => "m_inspectorController->applicationCacheAgent()",
};
$typeTransform{"Frontend"} = {
    "forward" => "RemoteInspectorFrontend",
    "header" => "RemoteInspectorFrontend.h",
};
$typeTransform{"PassRefPtr"} = {
    "forwardHeader" => "wtf/PassRefPtr.h",
};
$typeTransform{"Object"} = {
    "param" => "PassRefPtr<InspectorObject>",
    "variable" => "RefPtr<InspectorObject>",
    "defaultValue" => "InspectorObject::create()",
    "forward" => "InspectorObject",
    "header" => "InspectorValues.h",
    "accessorSuffix" => "Object"
};
$typeTransform{"Array"} = {
    "param" => "PassRefPtr<InspectorArray>",
    "variable" => "RefPtr<InspectorArray>",
    "defaultValue" => "InspectorArray::create()",
    "forward" => "InspectorArray",
    "header" => "InspectorValues.h",
    "accessorSuffix" => "Array"
};
$typeTransform{"Value"} = {
    "param" => "PassRefPtr<InspectorValue>",
    "variable" => "RefPtr<InspectorValue>",
    "defaultValue" => "InspectorValue::null()",
    "forward" => "InspectorValue",
    "header" => "InspectorValues.h",
    "accessorSuffix" => "Value"
};
$typeTransform{"String"} = {
    "param" => "const String&",
    "variable" => "String",
    "forwardHeader" => "wtf/Forward.h",
    "header" => "PlatformString.h",
    "accessorSuffix" => "String"
};
$typeTransform{"long"} = {
    "param" => "long",
    "variable" => "long",
    "defaultValue" => "0",
    "forward" => "",
    "header" => "",
    "accessorSuffix" => "Number"
};
$typeTransform{"int"} = {
    "param" => "int",
    "variable" => "int",
    "defaultValue" => "0",
    "forward" => "",
    "header" => "",
    "accessorSuffix" => "Number",
};
$typeTransform{"unsigned long"} = {
    "param" => "unsigned long",
    "variable" => "unsigned long",
    "defaultValue" => "0u",
    "forward" => "",
    "header" => "",
    "accessorSuffix" => "Number"
};
$typeTransform{"unsigned int"} = {
    "param" => "unsigned int",
    "variable" => "unsigned int",
    "defaultValue" => "0u",
    "forward" => "",
    "header" => "",
    "accessorSuffix" => "Number"
};
$typeTransform{"boolean"} = {
    "param" => "bool",
    "variable"=> "bool",
    "defaultValue" => "false",
    "forward" => "",
    "header" => "",
    "accessorSuffix" => "Bool"
};
$typeTransform{"void"} = {
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
my $backendJSStubName;
my %backendTypes;
my %backendMethods;
my @backendMethodsImpl;
my $backendConstructor;
my @backendConstantDeclarations;
my @backendConstantDefinitions;
my $backendFooter;
my @backendStubJS;

my $frontendClassName;
my %frontendTypes;
my %frontendMethods;
my @frontendMethodsImpl;
my $frontendConstructor;
my @frontendConstantDeclarations;
my @frontendConstantDefinitions;
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
    $backendJSStubName = $className . "BackendStub";
    my @backendHead;
    push(@backendHead, "    ${backendClassName}(InspectorController* inspectorController) : m_inspectorController(inspectorController) { }");
    push(@backendHead, "    void reportProtocolError(const long callId, const String& method, const String& errorText) const;");
    push(@backendHead, "    void dispatch(const String& message);");
    push(@backendHead, "    static bool getCommandName(const String& message, String* result);");
    $backendConstructor = join("\n", @backendHead);
    $backendFooter = "    InspectorController* m_inspectorController;";
    $backendTypes{"Controller"} = 1;
    $backendTypes{"InspectorClient"} = 1;
    $backendTypes{"PassRefPtr"} = 1;
    $backendTypes{"Array"} = 1;

    push(@backendMethodsImpl, generateBackendPrivateFunctions());
    push(@backendMethodsImpl, generateBackendMessageParser());
    generateFunctions($interface);

    # Make dispatcher methods private on the backend.
    push(@backendConstantDeclarations, "");
    push(@backendConstantDeclarations, "private:");
}

sub generateFunctions
{
    my $interface = shift;

    foreach my $function (@{$interface->functions}) {
        if ($function->signature->extendedAttributes->{"notify"}) {
            generateFrontendFunction($function);
        } else {
            generateBackendFunction($function);
        }
    }
    push(@backendMethodsImpl, generateBackendDispatcher());
    push(@backendMethodsImpl, generateBackendReportProtocolError());

    @backendStubJS = generateBackendStubJS($interface);
}

sub generateFrontendFunction
{
    my $function = shift;

    my $functionName = $function->signature->name;

    my @argsFiltered = grep($_->direction eq "out", @{$function->parameters}); # just keep only out parameters for frontend interface.
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
    return String::format("Wrong number of parameters: %d (expected: %d)", actual, expected);
}

static String formatWrongArgumentTypeMessage(unsigned position, const char* name, const char* expectedType)
{
    return String::format("Failed to convert parameter %d (%s) to %s", position, name, expectedType);
}
EOF
    return split("\n", $privateFunctions);
}

sub generateBackendFunction
{
    my $function = shift;

    my $functionName = $function->signature->name;

    push(@backendConstantDeclarations, "    static const char* ${functionName}Cmd;");
    push(@backendConstantDefinitions, "const char* ${backendClassName}::${functionName}Cmd = \"${functionName}\";");

    map($backendTypes{$_->type} = 1, @{$function->parameters}); # register required types
    my @inArgs = grep($_->direction eq "in", @{$function->parameters});
    my @outArgs = grep($_->direction eq "out", @{$function->parameters});

    my $signature = "    void ${functionName}(PassRefPtr<InspectorArray> args);";
    !$backendMethods{${signature}} || die "Duplicate function was detected for signature '$signature'.";
    $backendMethods{${signature}} = $functionName;

    my @function;
    push(@function, "void ${backendClassName}::${functionName}(PassRefPtr<InspectorArray> args)");
    push(@function, "{");
    push(@function, "    long callId = 0;");
    push(@function, "");

    my $expectedParametersCount = scalar(@inArgs);
    my $expectedParametersCountWithMethodName = scalar(@inArgs) + 1;
    push(@function, "    if (args->length() != $expectedParametersCountWithMethodName) {");
    push(@function, "        ASSERT_NOT_REACHED();");
    push(@function, "        reportProtocolError(callId, ${functionName}Cmd, formatWrongArgumentsCountMessage(args->length() - 1, $expectedParametersCount));");
    push(@function, "        return;");
    push(@function, "    }");
    push(@function, "");

    my $i = 1; # zero element is the method name.
    foreach my $parameter (@inArgs) {
        my $type = $parameter->type;
        my $argumentType = $typeTransform{$type}->{"variable"};
        push(@function, "    $argumentType " . $parameter->name . ";") if !($parameter->name eq "callId");
        push(@function, "    if (!args->get($i)->as" . $typeTransform{$type}->{"accessorSuffix"} . "(&" . $parameter->name . ")) {");
        push(@function, "        ASSERT_NOT_REACHED();");
        push(@function, "        reportProtocolError(callId, ${functionName}Cmd, formatWrongArgumentTypeMessage($i, \"" . $parameter->name . "\", \"$type\"));");
        push(@function, "        return;");
        push(@function, "    }");
        push(@function, "");
        ++$i;
    }

    my $handler = $function->signature->extendedAttributes->{"handler"} || "Controller";
    my $handlerAccessor = $typeTransform{$handler}->{"handlerAccessor"};
    $backendTypes{$handler} = 1;
    push(@function, "    if (!$handlerAccessor) {");
    push(@function, "        reportProtocolError(callId, ${functionName}Cmd, \"Error: $handler handler is not available.\");");
    push(@function, "        return;");
    push(@function, "    }");
    push(@function, "");


    foreach (@outArgs) { # declare local variables for out arguments.
        my $initializer = $typeTransform{$_->type}->{"defaultValue"} ? " = " . $typeTransform{$_->type}->{"defaultValue"} : "";
        push(@function, "    " . $typeTransform{$_->type}->{"variable"} . " " . $_->name . "$initializer;");
    }

    my $args = join(", ", (grep(!($_ eq "callId"), map($_->name, @inArgs)), map("&" . $_->name, @outArgs)));
    push(@function, "    $handlerAccessor->$functionName($args);");

    # The results of function call should be transfered back to frontend.
    if (scalar(grep($_->name eq "callId", @inArgs))) {
        my @pushArguments = map("        arguments->push" . $typeTransform{$_->type}->{"accessorSuffix"} . "(" . $_->name . ");", @outArgs);

        push(@function, "");
        push(@function, "    // use InspectorFrontend as a marker of WebInspector availability");
        push(@function, "    if (m_inspectorController->hasFrontend()) {");
        push(@function, "        RefPtr<InspectorArray> arguments = InspectorArray::create();");
        push(@function, "        arguments->pushString(\"processResponse\");");
        push(@function, "        arguments->pushNumber(callId);");
        push(@function, @pushArguments);
        push(@function, "        m_inspectorController->inspectorClient()->sendMessageToFrontend(arguments->toJSONString());");
        push(@function, "    }");
    }
    push(@function, "}");
    push(@function, "");
    push(@backendMethodsImpl, @function);
}

sub generateBackendReportProtocolError
{
    my $reportProtocolError = << "EOF";

void ${backendClassName}::reportProtocolError(const long callId, const String& method, const String& errorText) const
{
    RefPtr<InspectorArray> arguments = InspectorArray::create();
    arguments->pushString("reportProtocolError");
    arguments->pushNumber(callId);
    arguments->pushString(method);
    arguments->pushString(errorText);
    m_inspectorController->inspectorClient()->sendMessageToFrontend(arguments->toJSONString());
}
EOF
    return split("\n", $reportProtocolError);
}

sub generateBackendDispatcher
{
    my @body;
    my @methods = map($backendMethods{$_}, keys %backendMethods);
    my @mapEntries = map("        dispatchMap.add(${_}Cmd, &${backendClassName}::$_);", @methods);
    my $mapEntries = join("\n", @mapEntries);

    my $backendDispatcherBody = << "EOF";
void ${backendClassName}::dispatch(const String& message)
{
    typedef void (${backendClassName}::*CallHandler)(PassRefPtr<InspectorArray> args);
    typedef HashMap<String, CallHandler> DispatchMap;
    DEFINE_STATIC_LOCAL(DispatchMap, dispatchMap, );
    if (dispatchMap.isEmpty()) {
$mapEntries
    }

    RefPtr<InspectorValue> parsedMessage = InspectorValue::parseJSON(message);
    if (!parsedMessage) {
        ASSERT_NOT_REACHED();
        reportProtocolError(0, "dispatch", "Error: Invalid message format. Message should be in JSON format.");
        return;
    }

    RefPtr<InspectorArray> messageArray = parsedMessage->asArray();
    if (!messageArray) {
        ASSERT_NOT_REACHED();
        reportProtocolError(0, "dispatch", "Error: Invalid message format. The message should be a JSONified array of arguments.");
        return;
    }

    if (!messageArray->length()) {
        ASSERT_NOT_REACHED();
        reportProtocolError(0, "dispatch", "Error: Invalid message format. Empty message was received.");
        return;
    }

    String methodName;
    if (!messageArray->get(0)->asString(&methodName)) {
        ASSERT_NOT_REACHED();
        reportProtocolError(0, "dispatch", "Error: Invalid message format. The first element of the message should be method name.");
        return;
    }

    HashMap<String, CallHandler>::iterator it = dispatchMap.find(methodName);
    if (it == dispatchMap.end()) {
        ASSERT_NOT_REACHED();
        reportProtocolError(0, "dispatch", String::format("Error: Invalid method name. '%s' wasn't found.", methodName.utf8().data()));
        return;
    }

    ((*this).*it->second)(messageArray);
}
EOF
    return split("\n", $backendDispatcherBody);
}

sub generateBackendMessageParser
{
    my $messageParserBody = << "EOF";
bool ${backendClassName}::getCommandName(const String& message, String* result)
{
    RefPtr<InspectorValue> value = InspectorValue::parseJSON(message);
    if (!value)
        return false;
    RefPtr<InspectorArray> array = value->asArray();
    if (!array)
        return false;

    if (!array->length())
        return false;
    return array->get(0)->asString(result);
}
EOF
    return split("\n", $messageParserBody);
}

sub generateBackendStubJS
{
    my $interface = shift;
    my @backendFunctions = grep(!$_->signature->extendedAttributes->{"notify"}, @{$interface->functions});
    my @JSStubs = map("    this._registerDelegate(\"" . $_->signature->name . "\");", @backendFunctions);

    my $JSStubs = join("\n", @JSStubs);
    my $inspectorBackendStubJS = << "EOF";
$licenseTemplate

WebInspector.InspectorBackendStub = function()
{
$JSStubs
}

WebInspector.InspectorBackendStub.prototype = {
    _registerDelegate: function(methodName)
    {
        this[methodName] = this.sendMessageToBackend.bind(this, methodName);
    },

    sendMessageToBackend: function()
    {
        var message = JSON.stringify(Array.prototype.slice.call(arguments));
        InspectorFrontendHost.sendMessageToBackend(message);
    }
}

InspectorBackend = new WebInspector.InspectorBackendStub();

EOF
    return split("\n", $inspectorBackendStubJS);
}

sub generateHeader
{
    my $className = shift;
    my $types = shift;
    my $constructor = shift;
    my $constants = shift;
    my $methods = shift;
    my $footer = shift;

    my $forwardHeaders = join("\n", sort(map("#include <" . $typeTransform{$_}->{"forwardHeader"} . ">", grep($typeTransform{$_}->{"forwardHeader"}, keys %{$types}))));
    my $forwardDeclarations = join("\n", sort(map("class " . $typeTransform{$_}->{"forward"} . ";", grep($typeTransform{$_}->{"forward"}, keys %{$types}))));
    my $constantDeclarations = join("\n", @{$constants});
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

$constantDeclarations
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
    my $constants = shift;
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
    push (@sourceContent, join("\n", @{$constants}));
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
    print $SOURCE join("\n", generateSource($frontendClassName, \%frontendTypes, \@frontendConstantDefinitions, \@frontendMethodsImpl));
    close($SOURCE);
    undef($SOURCE);

    open(my $HEADER, ">$outputHeadersDir/$frontendClassName.h") || die "Couldn't open file $outputHeadersDir/$frontendClassName.h";
    print $HEADER generateHeader($frontendClassName, \%frontendTypes, $frontendConstructor, \@frontendConstantDeclarations, \%frontendMethods, $frontendFooter);
    close($HEADER);
    undef($HEADER);

    open($SOURCE, ">$outputDir/$backendClassName.cpp") || die "Couldn't open file $outputDir/$backendClassName.cpp";
    print $SOURCE join("\n", generateSource($backendClassName, \%backendTypes, \@backendConstantDefinitions, \@backendMethodsImpl));
    close($SOURCE);
    undef($SOURCE);

    open($HEADER, ">$outputHeadersDir/$backendClassName.h") || die "Couldn't open file $outputHeadersDir/$backendClassName.h";
    print $HEADER join("\n", generateHeader($backendClassName, \%backendTypes, $backendConstructor, \@backendConstantDeclarations, \%backendMethods, $backendFooter));
    close($HEADER);
    undef($HEADER);

    open(my $JS_STUB, ">$outputDir/$backendJSStubName.js") || die "Couldn't open file $outputDir/$backendJSStubName.js";
    print $JS_STUB join("\n", @backendStubJS);
    close($JS_STUB);
    undef($JS_STUB);
}

1;
