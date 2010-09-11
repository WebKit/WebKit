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
    "domainAccessor" => "m_inspectorController->inspectorBackend()",
};
$typeTransform{"Controller"} = {
    "forwardHeader" => "InspectorController.h",
    "domainAccessor" => "m_inspectorController",
};
$typeTransform{"Debug"} = {
    "forward" => "InspectorDebuggerAgent",
    "header" => "InspectorDebuggerAgent.h",
    "domainAccessor" => "m_inspectorController->debuggerAgent()",
};
$typeTransform{"DOM"} = {
    "forward" => "InspectorDOMAgent",
    "header" => "InspectorDOMAgent.h",
    "domainAccessor" => "m_inspectorController->domAgent()",
};
$typeTransform{"ApplicationCache"} = {
    "forward" => "InspectorApplicationCacheAgent",
    "header" => "InspectorApplicationCacheAgent.h",
    "domainAccessor" => "m_inspectorController->applicationCacheAgent()",
};
$typeTransform{"Profiler"} = {
    "forward" => "InspectorProfilerAgent",
    "header" => "InspectorProfilerAgent.h",
    "domainAccessor" => "m_inspectorController->profilerAgent()",
};
$typeTransform{"Frontend"} = {
    "forward" => "InspectorFrontend",
    "header" => "InspectorFrontend.h",
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
    "JSONType" => "Object"
};
$typeTransform{"Array"} = {
    "param" => "PassRefPtr<InspectorArray>",
    "variable" => "RefPtr<InspectorArray>",
    "defaultValue" => "InspectorArray::create()",
    "forward" => "InspectorArray",
    "header" => "InspectorValues.h",
    "JSONType" => "Array"
};
$typeTransform{"Value"} = {
    "param" => "PassRefPtr<InspectorValue>",
    "variable" => "RefPtr<InspectorValue>",
    "defaultValue" => "InspectorValue::null()",
    "forward" => "InspectorValue",
    "header" => "InspectorValues.h",
    "JSONType" => "Value"
};
$typeTransform{"String"} = {
    "param" => "const String&",
    "variable" => "String",
    "defaultValue" => "\"\"",
    "forwardHeader" => "wtf/Forward.h",
    "header" => "PlatformString.h",
    "JSONType" => "String"
};
$typeTransform{"long"} = {
    "param" => "long",
    "variable" => "long",
    "defaultValue" => "0",
    "forward" => "",
    "header" => "",
    "JSONType" => "Number"
};
$typeTransform{"int"} = {
    "param" => "int",
    "variable" => "int",
    "defaultValue" => "0",
    "forward" => "",
    "header" => "",
    "JSONType" => "Number",
};
$typeTransform{"unsigned long"} = {
    "param" => "unsigned long",
    "variable" => "unsigned long",
    "defaultValue" => "0u",
    "forward" => "",
    "header" => "",
    "JSONType" => "Number"
};
$typeTransform{"unsigned int"} = {
    "param" => "unsigned int",
    "variable" => "unsigned int",
    "defaultValue" => "0u",
    "forward" => "",
    "header" => "",
    "JSONType" => "Number"
};
$typeTransform{"boolean"} = {
    "param" => "bool",
    "variable"=> "bool",
    "defaultValue" => "false",
    "forward" => "",
    "header" => "",
    "JSONType" => "Boolean"
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

    $frontendClassName = $className . "Frontend";
    $frontendConstructor = "    ${frontendClassName}(InspectorClient* inspectorClient) : m_inspectorClient(inspectorClient) { }";
    $frontendFooter = "    InspectorClient* m_inspectorClient;";
    $frontendTypes{"String"} = 1;
    $frontendTypes{"InspectorClient"} = 1;
    $frontendTypes{"PassRefPtr"} = 1;

    $backendClassName = $className . "BackendDispatcher";
    $backendJSStubName = $className . "BackendStub";
    my @backendHead;
    push(@backendHead, "    ${backendClassName}(InspectorController* inspectorController) : m_inspectorController(inspectorController) { }");
    push(@backendHead, "    void reportProtocolError(const long callId, const String& errorText) const;");
    push(@backendHead, "    void dispatch(const String& message);");
    push(@backendHead, "    static bool getCommandName(const String& message, String* result);");
    $backendConstructor = join("\n", @backendHead);
    $backendFooter = "    InspectorController* m_inspectorController;";
    $backendTypes{"Controller"} = 1;
    $backendTypes{"InspectorClient"} = 1;
    $backendTypes{"PassRefPtr"} = 1;
    $backendTypes{"Object"} = 1;

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

    my $signature = "    void ${functionName}(${arguments});";
    if (!$frontendMethods{${signature}}) {
        $frontendMethods{${signature}} = 1;

        my @function;
        push(@function, "void ${frontendClassName}::${functionName}(${arguments})");
        push(@function, "{");
        push(@function, "    RefPtr<InspectorObject> ${functionName}Message = InspectorObject::create();");
        push(@function, "    ${functionName}Message->setString(\"type\", \"event\");");
        push(@function, "    ${functionName}Message->setString(\"event\", \"$functionName\");");
        push(@function, "    RefPtr<InspectorObject> payloadDataObject = InspectorObject::create();");
        my @pushArguments = map("    payloadDataObject->set" . $typeTransform{$_->type}->{"JSONType"} . "(\"" . $_->name . "\", " . $_->name . ");", @argsFiltered);
        push(@function, @pushArguments);
        push(@function, "    ${functionName}Message->setObject(\"data\", payloadDataObject);");
        push(@function, "    m_inspectorClient->sendMessageToFrontend(${functionName}Message->toJSONString());");

        push(@function, "}");
        push(@function, "");
        push(@frontendMethodsImpl, @function);
    }
}

sub generateBackendFunction
{
    my $function = shift;

    my $functionName = $function->signature->name;

    push(@backendConstantDeclarations, "    static const char* ${functionName}Cmd;");
    push(@backendConstantDefinitions, "const char* ${backendClassName}::${functionName}Cmd = \"${functionName}\";");

    map($backendTypes{$_->type} = 1, @{$function->parameters}); # register required types
    my @inArgs = grep($_->direction eq "in" && !($_->name eq "callId") , @{$function->parameters});
    my @outArgs = grep($_->direction eq "out", @{$function->parameters});

    my $signature = "    void ${functionName}(long callId, InspectorObject* requestMessageObject);";
    !$backendMethods{${signature}} || die "Duplicate function was detected for signature '$signature'.";
    $backendMethods{${signature}} = $functionName;

    my @function;
    my $requestMessageObject = scalar(@inArgs) ? " requestMessageObject" : "";
    push(@function, "void ${backendClassName}::${functionName}(long callId, InspectorObject*$requestMessageObject)");
    push(@function, "{");
    push(@function, "    RefPtr<InspectorArray> protocolErrors = InspectorArray::create();");
    push(@function, "");

    my $domain = $function->signature->extendedAttributes->{"handler"} || "Controller";
    my $domainAccessor = $typeTransform{$domain}->{"domainAccessor"};
    $backendTypes{$domain} = 1;
    push(@function, "    if (!$domainAccessor)");
    push(@function, "        protocolErrors->pushString(String::format(\"Protocol Error: %s handler is not available.\", \"$domain\"));");
    push(@function, "");

    if (scalar(@inArgs)) {
        # declare variables for all 'in' args;
        push(@function, map("    " . $typeTransform{$_->type}->{"variable"} . " " . $_->name . " = " . $typeTransform{$_->type}->{"defaultValue"} . ";", @inArgs));

        push(@function, "");
        push(@function, "    RefPtr<InspectorObject> argumentsContainer;");
        push(@function, "    if (!(argumentsContainer = requestMessageObject->getObject(\"arguments\"))) {");
        push(@function, "        protocolErrors->pushString(String::format(\"Protocol Error: 'arguments' property with type 'object' was not found.\"));");
        push(@function, "    } else {");
        push(@function, "        InspectorObject::const_iterator argumentsEndIterator = argumentsContainer->end();");

        foreach my $parameter (@inArgs) {
            my $name = $parameter->name;
            my $type = $parameter->type;
            my $variableType = $typeTransform{$type}->{"variable"};
            my $JSONType = $typeTransform{$type}->{"JSONType"};

            push(@function, "");
            push(@function, "        InspectorObject::const_iterator ${name}ValueIterator = argumentsContainer->find(\"$name\");");
            push(@function, "        if (${name}ValueIterator == argumentsEndIterator) {");
            push(@function, "            protocolErrors->pushString(String::format(\"Protocol Error: Argument '%s' with type '%s' was not found.\", \"$name\", \"$JSONType\"));");
            push(@function, "        } else {");
            push(@function, "            if (!${name}ValueIterator->second->as$JSONType(&$name)) {");
            push(@function, "                protocolErrors->pushString(String::format(\"Protocol Error: Argument '%s' has wrong type. It should be '%s'.\", \"$name\", \"$JSONType\"));");
            push(@function, "            }");
            push(@function, "        }");
        }
        push(@function, "    }");
    }

    # declare local variables for out arguments.
    push(@function, map("    " . $typeTransform{$_->type}->{"variable"} . " " . $_->name . " = " . $typeTransform{$_->type}->{"defaultValue"} . ";", @outArgs));

    my $args = join(", ", (map($_->name, @inArgs), map("&" . $_->name, @outArgs)));
    push(@function, "    if (!protocolErrors->length())");
    push(@function, "        $domainAccessor->$functionName($args);");
    push(@function, "");

    push(@function, "    // use InspectorFrontend as a marker of WebInspector availability");
    push(@function, "    if ((callId || protocolErrors->length()) && m_inspectorController->hasFrontend()) {");
    push(@function, "        RefPtr<InspectorObject> responseMessage = InspectorObject::create();");
    push(@function, "        responseMessage->setNumber(\"seq\", callId);");
    push(@function, "        responseMessage->setBoolean(\"success\", !protocolErrors->length());");
    push(@function, "");
    push(@function, "        if (protocolErrors->length())");
    push(@function, "            responseMessage->setArray(\"errors\", protocolErrors);");
    if (scalar(@outArgs)) {
        push(@function, "        else {");
        push(@function, "            RefPtr<InspectorObject> responseData = InspectorObject::create();");
        push(@function, map("            responseData->set" . $typeTransform{$_->type}->{"JSONType"} . "(\"" . $_->name . "\", " . $_->name . ");", @outArgs));
        push(@function, "            responseMessage->setObject(\"data\", responseData);");
        push(@function, "        }");
    }
    push(@function, "        m_inspectorController->inspectorClient()->sendMessageToFrontend(responseMessage->toJSONString());");
    push(@function, "    }");


    push(@function, "}");
    push(@function, "");
    push(@backendMethodsImpl, @function);
}

sub generateBackendReportProtocolError
{
    my $reportProtocolError = << "EOF";

void ${backendClassName}::reportProtocolError(const long callId, const String& errorText) const
{
    RefPtr<InspectorObject> message = InspectorObject::create();
    message->setNumber("seq", callId);
    message->setBoolean("success", false);
    RefPtr<InspectorArray> errors = InspectorArray::create();
    errors->pushString(errorText);
    message->setArray("errors", errors);
    m_inspectorController->inspectorClient()->sendMessageToFrontend(message->toJSONString());
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
    typedef void (${backendClassName}::*CallHandler)(long callId, InspectorObject* messageObject);
    typedef HashMap<String, CallHandler> DispatchMap;
    DEFINE_STATIC_LOCAL(DispatchMap, dispatchMap, );
    long callId = 0;

    if (dispatchMap.isEmpty()) {
$mapEntries
    }

    RefPtr<InspectorValue> parsedMessage = InspectorValue::parseJSON(message);
    if (!parsedMessage) {
        reportProtocolError(callId, "Protocol Error: Invalid message format. Message should be in JSON format.");
        return;
    }

    RefPtr<InspectorObject> messageObject = parsedMessage->asObject();
    if (!messageObject) {
        reportProtocolError(callId, "Protocol Error: Invalid message format. The message should be a JSONified object.");
        return;
    }

    RefPtr<InspectorValue> commandValue = messageObject->get("command");
    if (!commandValue) {
        reportProtocolError(callId, "Protocol Error: Invalid message format. 'command' property wasn't found.");
        return;
    }

    String command;
    if (!commandValue->asString(&command)) {
        reportProtocolError(callId, "Protocol Error: Invalid message format. The type of 'command' property should be string.");
        return;
    }

    RefPtr<InspectorValue> callIdValue = messageObject->get("seq");
    if (!callIdValue) {
        reportProtocolError(callId, "Protocol Error: Invalid message format. 'seq' property was not found in the request.");
        return;
    }

    if (!callIdValue->asNumber(&callId)) {
        reportProtocolError(callId, "Protocol Error: Invalid message format. The type of 'seq' property should be number.");
        return;
    }

    HashMap<String, CallHandler>::iterator it = dispatchMap.find(command);
    if (it == dispatchMap.end()) {
        reportProtocolError(callId, String::format("Protocol Error: Invalid command was received. '%s' wasn't found.", command.utf8().data()));
        return;
    }

    ((*this).*it->second)(callId, messageObject.get());
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

    RefPtr<InspectorObject> object = value->asObject();
    if (!object)
        return false;

    RefPtr<InspectorValue> commandValue = object->get("command");
    if (!commandValue)
        return false;

    return commandValue->asString(result);
}

EOF
    return split("\n", $messageParserBody);
}

sub generateBackendStubJS
{
    my $interface = shift;
    my @backendFunctions = grep(!$_->signature->extendedAttributes->{"notify"}, @{$interface->functions});
    my @JSStubs;

    foreach my $function (@backendFunctions) {
        my $name = $function->signature->name;
        my $domain = $function->signature->extendedAttributes->{"handler"};
        my $argumentNames = join(",", map("\"" . $_->name . "\": \"" . lc($typeTransform{$_->type}->{"JSONType"}) . "\"", grep($_->direction eq "in", @{$function->parameters})));
        push(@JSStubs, "    this._registerDelegate('{" .
            "\"seq\": 0, " .
            "\"domain\": \"$domain\", " .
            "\"command\": \"$name\", " .
            "\"arguments\": {$argumentNames}" .
        "}');");
    }

    my $JSStubs = join("\n", @JSStubs);
    my $inspectorBackendStubJS = << "EOF";
$licenseTemplate

WebInspector.InspectorBackendStub = function()
{
$JSStubs
}

WebInspector.InspectorBackendStub.prototype = {
    _registerDelegate: function(commandInfo)
    {
        var commandObject = JSON.parse(commandInfo);
        this[commandObject.command] = this.sendMessageToBackend.bind(this, commandInfo);
    },

    sendMessageToBackend: function()
    {
        var args = Array.prototype.slice.call(arguments);
        var request = JSON.parse(args.shift());

        for (var key in request.arguments) {
            if (args.length === 0) {
                console.error("Protocol Error: Invalid number of arguments for 'InspectorBackend.%s' call. It should have the next arguments '%s'.", request.command, JSON.stringify(request.arguments));
                return;
            }
            var value = args.shift();
            if (typeof value !== request.arguments[key]) {
                console.error("Protocol Error: Invalid type of argument '%s' for 'InspectorBackend.%s' call. It should be '%s' but it is '%s'.", key, request.command, request.arguments[key], typeof value);
                return;
            }
            request.arguments[key] = value;
        }

        if (args.length === 1) {
            if (typeof args[0] !== "function" && typeof args[0] !== "undefined") {
                console.error("Protocol Error: Optional callback argument for 'InspectorBackend.%s' call should be a function but its type is '%s'.", request.command, typeof args[0]);
                return;
            }
            request.seq = WebInspector.Callback.wrap(args[0]);
        }

        var message = JSON.stringify(request);
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
    push(@sourceContent, "#include <wtf/text/CString.h>");
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
