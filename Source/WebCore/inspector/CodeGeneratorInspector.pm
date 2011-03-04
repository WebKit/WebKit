# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

package CodeGeneratorInspector;

use strict;

use Class::Struct;
use File::stat;

my %typeTransform;
$typeTransform{"ApplicationCache"} = {
    "forward" => "InspectorApplicationCacheAgent",
    "header" => "InspectorApplicationCacheAgent.h",
    "domainAccessor" => "m_inspectorAgent->applicationCacheAgent()",
};
$typeTransform{"CSS"} = {
    "forward" => "InspectorCSSAgent",
    "header" => "InspectorCSSAgent.h",
    "domainAccessor" => "m_inspectorAgent->cssAgent()",
};
$typeTransform{"Console"} = {
    "forward" => "InspectorConsoleAgent",
    "header" => "InspectorConsoleAgent.h",
    "domainAccessor" => "m_inspectorAgent->consoleAgent()",
};
$typeTransform{"Debugger"} = {
    "forward" => "InspectorDebuggerAgent",
    "header" => "InspectorDebuggerAgent.h",
    "domainAccessor" => "m_inspectorAgent->debuggerAgent()",
};
$typeTransform{"BrowserDebugger"} = {
    "forward" => "InspectorBrowserDebuggerAgent",
    "header" => "InspectorBrowserDebuggerAgent.h",
    "domainAccessor" => "m_inspectorAgent->browserDebuggerAgent()",
};
$typeTransform{"Database"} = {
    "forward" => "InspectorDatabaseAgent",
    "header" => "InspectorDatabaseAgent.h",
    "domainAccessor" => "m_inspectorAgent->databaseAgent()",
};
$typeTransform{"DOM"} = {
    "forward" => "InspectorDOMAgent",
    "header" => "InspectorDOMAgent.h",
    "domainAccessor" => "m_inspectorAgent->domAgent()",
};
$typeTransform{"DOMStorage"} = {
    "forward" => "InspectorDOMStorageAgent",
    "header" => "InspectorDOMStorageAgent.h",
    "domainAccessor" => "m_inspectorAgent->domStorageAgent()",
};
$typeTransform{"FileSystem"} = {
    "forward" => "InspectorFileSystemAgent",
    "header" => "InspectorFileSystemAgent.h",
    "domainAccessor" => "m_inspectorAgent->fileSystemAgent()",
};
$typeTransform{"Inspector"} = {
    "forwardHeader" => "InspectorAgent.h",
    "domainAccessor" => "m_inspectorAgent",
};
$typeTransform{"Network"} = {
    "forward" => "InspectorResourceAgent",
    "header" => "InspectorResourceAgent.h",
    "domainAccessor" => "m_inspectorAgent->resourceAgent()",
};
$typeTransform{"Profiler"} = {
    "forward" => "InspectorProfilerAgent",
    "header" => "InspectorProfilerAgent.h",
    "domainAccessor" => "m_inspectorAgent->profilerAgent()",
};
$typeTransform{"Runtime"} = {
    "forward" => "InspectorRuntimeAgent",
    "header" => "InspectorRuntimeAgent.h",
    "domainAccessor" => "m_inspectorAgent->runtimeAgent()",
};
$typeTransform{"Timeline"} = {
    "forward" => "InspectorTimelineAgent",
    "header" => "InspectorTimelineAgent.h",
    "domainAccessor" => "m_inspectorAgent->timelineAgent()",
};

$typeTransform{"Frontend"} = {
    "forward" => "InspectorFrontend",
    "header" => "InspectorFrontend.h",
};
$typeTransform{"InspectorClient"} = {
    "forward" => "InspectorClient",
    "header" => "InspectorClient.h",
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
    "JSONType" => "Object",
    "JSType" => "object",
    "DocType" => "%s"
};
$typeTransform{"Array"} = {
    "param" => "PassRefPtr<InspectorArray>",
    "variable" => "RefPtr<InspectorArray>",
    "defaultValue" => "InspectorArray::create()",
    "forward" => "InspectorArray",
    "header" => "InspectorValues.h",
    "JSONType" => "Array",
    "JSType" => "object",
    "DocType" => "array of %s"
};
$typeTransform{"Value"} = {
    "param" => "PassRefPtr<InspectorValue>",
    "variable" => "RefPtr<InspectorValue>",
    "defaultValue" => "InspectorValue::null()",
    "forward" => "InspectorValue",
    "header" => "InspectorValues.h",
    "JSONType" => "Value",
    "JSType" => "",
    "DocType" => "value"
};
$typeTransform{"String"} = {
    "param" => "const String&",
    "variable" => "String",
    "return" => "String",
    "defaultValue" => "\"\"",
    "forwardHeader" => "PlatformString.h",
    "header" => "PlatformString.h",
    "JSONType" => "String",
    "JSType" => "string"
};
$typeTransform{"long"} = {
    "param" => "long",
    "variable" => "long",
    "defaultValue" => "0",
    "forward" => "",
    "header" => "",
    "JSONType" => "Number",
    "JSType" => "number"
};
$typeTransform{"int"} = {
    "param" => "int",
    "variable" => "int",
    "defaultValue" => "0",
    "forward" => "",
    "header" => "",
    "JSONType" => "Number",
    "JSType" => "number"
};
$typeTransform{"unsigned long"} = {
    "param" => "unsigned long",
    "variable" => "unsigned long",
    "defaultValue" => "0u",
    "forward" => "",
    "header" => "",
    "JSONType" => "Number",
    "JSType" => "number"
};
$typeTransform{"unsigned int"} = {
    "param" => "unsigned int",
    "variable" => "unsigned int",
    "defaultValue" => "0u",
    "forward" => "",
    "header" => "",
    "JSONType" => "Number",
    "JSType" => "number"
};
$typeTransform{"double"} = {
    "param" => "double",
    "variable" => "double",
    "defaultValue" => "0.0",
    "forward" => "",
    "header" => "",
    "JSONType" => "Number",
    "JSType" => "number"
};
$typeTransform{"boolean"} = {
    "param" => "bool",
    "variable"=> "bool",
    "defaultValue" => "false",
    "forward" => "",
    "header" => "",
    "JSONType" => "Boolean",
    "JSType" => "boolean"
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
my @backendMethods;
my @backendMethodsImpl;
my %backendMethodSignatures;
my $backendConstructor;
my @backendConstantDeclarations;
my @backendConstantDefinitions;
my $backendFooter;
my @backendJSStubs;

my $frontendClassName;
my %frontendTypes;
my @frontendMethods;
my @frontendAgentFields;
my @frontendMethodsImpl;
my %frontendMethodSignatures;
my $frontendConstructor;
my @frontendConstantDeclarations;
my @frontendConstantDefinitions;
my @frontendFooter;

my @documentationToc;
my @documentationLines;

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

    $frontendClassName = "InspectorFrontend";
    $frontendConstructor = "    ${frontendClassName}(InspectorClient*);";
    push(@frontendFooter, "private:");
    push(@frontendFooter, "    InspectorClient* m_inspectorClient;");
    $frontendTypes{"String"} = 1;
    $frontendTypes{"InspectorClient"} = 1;
    $frontendTypes{"PassRefPtr"} = 1;

    $backendClassName = "InspectorBackendDispatcher";
    $backendJSStubName = "InspectorBackendStub";
    my @backendHead;
    push(@backendHead, "    ${backendClassName}(InspectorAgent* inspectorAgent) : m_inspectorAgent(inspectorAgent) { }");
    push(@backendHead, "    void reportProtocolError(const long callId, const String& errorText) const;");
    push(@backendHead, "    void dispatch(const String& message);");
    push(@backendHead, "    static bool getCommandName(const String& message, String* result);");
    $backendConstructor = join("\n", @backendHead);
    $backendFooter = "    InspectorAgent* m_inspectorAgent;";
    $backendTypes{"Inspector"} = 1;
    $backendTypes{"InspectorClient"} = 1;
    $backendTypes{"PassRefPtr"} = 1;
    $backendTypes{"Object"} = 1;
}

# Params: 'idlDocument' struct
sub GenerateInterface
{
    my $object = shift;
    my $interface = shift;
    my $defines = shift;

    my %agent = (
        methodDeclarations => [],
        methodSignatures => {}
    );
    generateFunctions($interface, \%agent);
    if (@{%agent->{methodDeclarations}}) {
        generateAgentDeclaration($interface, \%agent);
    }
}

sub generateAgentDeclaration
{
    my $interface = shift;
    my $agent = shift;
    my $agentName = $interface->name;
    push(@frontendMethods, "    class ${agentName} {");
    push(@frontendMethods, "    public:");
    push(@frontendMethods, "        ${agentName}(InspectorClient* inspectorClient) : m_inspectorClient(inspectorClient) { }");
    push(@frontendMethods, @{$agent->{methodDeclarations}});
    push(@frontendMethods, "    private:");
    push(@frontendMethods, "        InspectorClient* m_inspectorClient;");
    push(@frontendMethods, "    };");
    push(@frontendMethods, "");

    my $getterName = lc($agentName);
    push(@frontendMethods, "    ${agentName}* ${getterName}() { return &m_${getterName}; }");
    push(@frontendMethods, "");

    push(@frontendFooter, "    ${agentName} m_${getterName};");

    push(@frontendAgentFields, "m_${getterName}");
}

sub generateFrontendConstructorImpl
{
    my @frontendConstructorImpl;
    push(@frontendConstructorImpl, "${frontendClassName}::${frontendClassName}(InspectorClient* inspectorClient)");
    push(@frontendConstructorImpl, "    : m_inspectorClient(inspectorClient)");
    foreach my $agentField (@frontendAgentFields) {
        push(@frontendConstructorImpl, "    , ${agentField}(inspectorClient)");
    }
    push(@frontendConstructorImpl, "{");
    push(@frontendConstructorImpl, "}");
    return @frontendConstructorImpl;
}

sub generateFunctions
{
    my $interface = shift;
    my $agent = shift;

    foreach my $function (@{$interface->functions}) {
        if ($function->signature->extendedAttributes->{"event"}) {
            generateFrontendFunction($interface, $function, $agent);
        } else {
            generateBackendFunction($interface, $function);
        }
    }

    push(@documentationToc, "<li><a href='#" . $interface->name . "'>" . $interface->name . "</a></li>");
    push(@documentationLines, "<h2 id='" . $interface->name . "'><a name=" . $interface->name . "></a>" . $interface->name . "</h2>");

    push(@documentationLines, "<h3>Events</h3>");
    foreach my $function (grep($_->signature->extendedAttributes->{"event"}, @{$interface->functions}) ) {
        generateDocumentationEvent($interface, $function);
    }

    push(@documentationLines, "<h3>Commands</h3>");
    foreach my $function (grep(!$_->signature->extendedAttributes->{"event"}, @{$interface->functions})) {
        generateDocumentationCommand($interface, $function);
    }

    collectBackendJSStubFunctions($interface);
}

sub generateFrontendFunction
{
    my $interface = shift;
    my $function = shift;
    my $agent = shift;

    my $functionName = $function->signature->name;

    my $domain = $interface->name;
    my @argsFiltered = grep($_->direction eq "out", @{$function->parameters}); # just keep only out parameters for frontend interface.
    map($frontendTypes{$_->type} = 1, @argsFiltered); # register required types.
    my $arguments = join(", ", map(typeTraits($_->type, "param") . " " . $_->name, @argsFiltered)); # prepare arguments for function signature.

    my $signature = "        void ${functionName}(${arguments});";
    !$agent->{methodSignatures}->{$signature} || die "Duplicate frontend function was detected for signature '$signature'.";
    $agent->{methodSignatures}->{$signature} = 1;
    push(@{$agent->{methodDeclarations}}, $signature);

    my @function;
    push(@function, "void ${frontendClassName}::${domain}::${functionName}(${arguments})");
    push(@function, "{");
    push(@function, "    RefPtr<InspectorObject> ${functionName}Message = InspectorObject::create();");
    push(@function, "    ${functionName}Message->setString(\"type\", \"event\");");
    push(@function, "    ${functionName}Message->setString(\"domain\", \"$domain\");");
    push(@function, "    ${functionName}Message->setString(\"event\", \"$functionName\");");
    push(@function, "    RefPtr<InspectorObject> bodyObject = InspectorObject::create();");
    my @pushArguments = map("    bodyObject->set" . typeTraits($_->type, "JSONType") . "(\"" . $_->name . "\", " . $_->name . ");", @argsFiltered);
    push(@function, @pushArguments);
    push(@function, "    ${functionName}Message->setObject(\"body\", bodyObject);");
    push(@function, "    m_inspectorClient->sendMessageToFrontend(${functionName}Message->toJSONString());");
    push(@function, "}");
    push(@function, "");
    push(@frontendMethodsImpl, @function);
}

sub generateDocumentationEvent
{
    my $interface = shift;
    my $function = shift;

    my $functionName = $function->signature->name;
    my $domain = $interface->name;

    my @argsFiltered = grep($_->direction eq "out", @{$function->parameters});

    my @lines;
    push(@lines, "<h4>" . $interface->name . "." . ${functionName} . "</h4>");
    my $doc = $function->signature->extendedAttributes->{"doc"};
    if ($doc) {
        push(@lines, $doc);
    }

    push(@lines, "<pre style='background: lightGrey; padding: 10px'>");
    push(@lines, "{");
    push(@lines, "    seq: &lt;number&gt;,");
    push(@lines, "    type: \"event\",");
    push(@lines, "    domain: \"$domain\",");
    if (scalar(@argsFiltered)) {
        push(@lines, "    event: \"${functionName}\",");
        push(@lines, "    data: {");
        my @parameters;
        foreach my $parameter (@argsFiltered) {
            push(@parameters, "        " . parameterDocLine($parameter));
        }
        push(@lines, join(",\n", @parameters));
        push(@lines, "    }");
    } else {
        push(@lines, "    event: \"${functionName}\"");
    }
    push(@lines, "}");
    push(@lines, "</pre>");
    push(@documentationLines, @lines);
}

sub camelCase
{
    my $value = shift;
    $value =~ s/\b(\w)/\U$1/g; # make a camel-case name for type name
    $value =~ s/ //g;
    return $value;
}

sub generateBackendFunction
{
    my $interface = shift;
    my $function = shift;

    my $functionName = $function->signature->name;
    my $fullQualifiedFunctionName = $interface->name . "_" . $function->signature->name;

    push(@backendConstantDeclarations, "    static const char* ${fullQualifiedFunctionName}Cmd;");
    push(@backendConstantDefinitions, "const char* ${backendClassName}::${fullQualifiedFunctionName}Cmd = \"${fullQualifiedFunctionName}\";");

    map($backendTypes{$_->type} = 1, @{$function->parameters}); # register required types
    my @inArgs = grep($_->direction eq "in" && !($_->name eq "callId") , @{$function->parameters});
    my @outArgs = grep($_->direction eq "out", @{$function->parameters});
    
    my $signature = "    void ${fullQualifiedFunctionName}(long callId, InspectorObject* requestMessageObject);";
    !$backendMethodSignatures{${signature}} || die "Duplicate function was detected for signature '$signature'.";
    $backendMethodSignatures{${signature}} = "$fullQualifiedFunctionName";
    push(@backendMethods, ${signature});

    my @function;
    my $requestMessageObject = scalar(@inArgs) ? " requestMessageObject" : "";
    push(@function, "void ${backendClassName}::${fullQualifiedFunctionName}(long callId, InspectorObject*$requestMessageObject)");
    push(@function, "{");
    push(@function, "    RefPtr<InspectorArray> protocolErrors = InspectorArray::create();");
    push(@function, "");

    my $domain = $interface->name;
    my $domainAccessor = typeTraits($domain, "domainAccessor");
    $backendTypes{$domain} = 1;
    push(@function, "    if (!$domainAccessor)");
    push(@function, "        protocolErrors->pushString(\"Protocol Error: $domain handler is not available.\");");
    push(@function, "");

    # declare local variables for out arguments.
    push(@function, map("    " . typeTraits($_->type, "variable") . " " . $_->name . " = " . typeTraits($_->type, "defaultValue") . ";", @outArgs));

    my $indent = "";
    if (scalar(@inArgs)) {
        push(@function, "    if (RefPtr<InspectorObject> argumentsContainer = requestMessageObject->getObject(\"arguments\")) {");

        foreach my $parameter (@inArgs) {
            my $name = $parameter->name;
            my $type = $parameter->type;
            my $typeString = camelCase($parameter->type);
            push(@function, "        " . typeTraits($type, "variable") . " $name = get$typeString(argumentsContainer.get(), \"$name\", protocolErrors.get());");
        }
        push(@function, "");
        $indent = "    ";
    }

    push(@function, "$indent    ErrorString error;");
    my $args = join(", ", ("&error", map($_->name, @inArgs), map("&" . $_->name, @outArgs)));
    push(@function, "$indent    if (!protocolErrors->length())");
    push(@function, "$indent        $domainAccessor->$functionName($args);");
    push(@function, "$indent    if (error.length())");
    push(@function, "$indent        protocolErrors->pushString(error);");
    if (scalar(@inArgs)) {
        push(@function, "    } else {");
        push(@function, "        protocolErrors->pushString(\"Protocol Error: 'arguments' property with type 'object' was not found.\");");
        push(@function, "    }");
    }

    push(@function, "    // use InspectorFrontend as a marker of WebInspector availability");
    push(@function, "    if ((callId || protocolErrors->length()) && m_inspectorAgent->hasFrontend()) {");
    push(@function, "        RefPtr<InspectorObject> responseMessage = InspectorObject::create();");
    push(@function, "        responseMessage->setNumber(\"seq\", callId);");
    push(@function, "");
    push(@function, "        if (protocolErrors->length())");
    push(@function, "            responseMessage->setArray(\"errors\", protocolErrors);");
    if (scalar(@outArgs)) {
        push(@function, "        else {");
        push(@function, "            RefPtr<InspectorObject> responseBody = InspectorObject::create();");
        push(@function, map("            responseBody->set" . typeTraits($_->type, "JSONType") . "(\"" . $_->name . "\", " . $_->name . ");", @outArgs));
        push(@function, "            responseMessage->setObject(\"body\", responseBody);");
        push(@function, "        }");
    }
    push(@function, "        m_inspectorAgent->inspectorClient()->sendMessageToFrontend(responseMessage->toJSONString());");
    push(@function, "    }");


    push(@function, "}");
    push(@function, "");
    push(@backendMethodsImpl, @function);
}

sub generateDocumentationCommand
{
    my $interface = shift;
    my $function = shift;

    my $functionName = $function->signature->name;
    my $domain = $interface->name;

    my @lines;

    push(@lines, "<h4>" . $interface->name . "." . ${functionName} . "</h4>");
    my $doc = $function->signature->extendedAttributes->{"doc"};
    if ($doc) {
        push(@lines, $doc);
    }

    my @inArgs = grep($_->direction eq "in" && !($_->name eq "callId") , @{$function->parameters});
    push(@lines, "<pre style='background: lightGrey; padding: 10px'>");
    push(@lines, "request: {");
    push(@lines, "    seq: &lt;number&gt;,");
    push(@lines, "    type: \"request\",");
    push(@lines, "    domain: \"" . $interface->name . "\",");
    if (scalar(@inArgs)) {
        push(@lines, "    command: \"${functionName}\",");
        push(@lines, "    arguments: {");
        my @parameters;
        foreach my $parameter (@inArgs) {
            push(@parameters, "        " . parameterDocLine($parameter));
        }
        push(@lines, join(",\n", @parameters));
        push(@lines, "    }");
    } else {
        push(@lines, "    command: \"${functionName}\"");
    }
    push(@lines, "}");

    my @outArgs = grep($_->direction eq "out", @{$function->parameters});    
    push(@lines, "");
    push(@lines, "response: {");
    push(@lines, "    seq: &lt;number&gt;,");
    if (scalar(@outArgs)) {
        push(@lines, "    type: \"response\",");
        push(@lines, "    body: {");
            my @parameters;
            foreach my $parameter (@outArgs) {
                push(@parameters, "        " . parameterDocLine($parameter));
            }
            push(@lines, join(",\n", @parameters));
        push(@lines, "    }");
    } else {
        push(@lines, "    type: \"response\"");
    }
    push(@lines, "}");
    push(@lines, "</pre>");

    push(@documentationLines, @lines);
}

sub generateBackendReportProtocolError
{
    my $reportProtocolError = << "EOF";

void ${backendClassName}::reportProtocolError(const long callId, const String& errorText) const
{
    RefPtr<InspectorObject> message = InspectorObject::create();
    message->setNumber("seq", callId);
    RefPtr<InspectorArray> errors = InspectorArray::create();
    errors->pushString(errorText);
    message->setArray("errors", errors);
    m_inspectorAgent->inspectorClient()->sendMessageToFrontend(message->toJSONString());
}
EOF
    return split("\n", $reportProtocolError);
}

sub generateArgumentGetters
{
    my $type = shift;
    my $json = typeTraits($type, "JSONType");
    my $variable = typeTraits($type, "variable");
    my $defaultValue = typeTraits($type, "defaultValue");
    my $return  = typeTraits($type, "return") ? typeTraits($type, "return") : typeTraits($type, "param");

    my $typeString = camelCase($type);
    push(@backendConstantDeclarations, "    $return get$typeString(InspectorObject* object, const String& name, InspectorArray* protocolErrors);");
    my $getterBody = << "EOF";

$return InspectorBackendDispatcher::get$typeString(InspectorObject* object, const String& name, InspectorArray* protocolErrors)
{
    ASSERT(object);
    ASSERT(protocolErrors);

    $variable value = $defaultValue;
    InspectorObject::const_iterator end = object->end();
    InspectorObject::const_iterator valueIterator = object->find(name);

    if (valueIterator == end)
        protocolErrors->pushString(String::format("Protocol Error: Argument '\%s' with type '$json' was not found.", name.utf8().data()));
    else {
        if (!valueIterator->second->as$json(&value))
            protocolErrors->pushString(String::format("Protocol Error: Argument '\%s' has wrong type. It should be '$json'.", name.utf8().data()));
    }
    return value;
}
EOF

    return split("\n", $getterBody);
}

sub generateBackendDispatcher
{
    my @body;
    my @mapEntries = map("        dispatchMap.add(${_}Cmd, &${backendClassName}::$_);", map ($backendMethodSignatures{$_}, @backendMethods));
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

    RefPtr<InspectorValue> domainValue = messageObject->get("domain");
    if (!domainValue) {
        reportProtocolError(callId, "Protocol Error: Invalid message format. 'domain' property wasn't found.");
        return;
    }

    String domain;
    if (!domainValue->asString(&domain)) {
        reportProtocolError(callId, "Protocol Error: Invalid message format. The type of 'domain' property should be string.");
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

    HashMap<String, CallHandler>::iterator it = dispatchMap.find(makeString(domain, "_", command));
    if (it == dispatchMap.end()) {
        reportProtocolError(callId, makeString("Protocol Error: Invalid command was received. '", command, "' wasn't found in domain ", domain, "."));
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

sub collectBackendJSStubFunctions
{
    my $interface = shift;
    my @functions = grep(!$_->signature->extendedAttributes->{"event"}, @{$interface->functions});
    my $domain = $interface->name;

    foreach my $function (@functions) {
        my $name = $function->signature->name;
        my $argumentNames = join(",", map("\"" . $_->name . "\": \"" . typeTraits($_->type, "JSType") . "\"", grep($_->direction eq "in", @{$function->parameters})));
        push(@backendJSStubs, "    this._registerDelegate('{" .
            "\"seq\": 0, " .
            "\"domain\": \"$domain\", " .
            "\"command\": \"$name\", " .
            "\"arguments\": {$argumentNames}" .
        "}');");
    }
}

sub generateBackendStubJS
{
    my $JSStubs = join("\n", @backendJSStubs);
    my $inspectorBackendStubJS = << "EOF";
$licenseTemplate

InspectorBackendStub = function()
{
    this._lastCallbackId = 1;
    this._pendingResponsesCount = 0;
    this._callbacks = {};
    this._domainDispatchers = {};
$JSStubs
}

InspectorBackendStub.prototype = {
    _wrap: function(callback)
    {
        var callbackId = this._lastCallbackId++;
        ++this._pendingResponsesCount;
        this._callbacks[callbackId] = callback || function() {};
        return callbackId;
    },

    _registerDelegate: function(commandInfo)
    {
        var commandObject = JSON.parse(commandInfo);
        var agentName = commandObject.domain + "Agent";
        if (!window[agentName])
            window[agentName] = {};
        window[agentName][commandObject.command] = this.sendMessageToBackend.bind(this, commandInfo);
    },

    sendMessageToBackend: function()
    {
        var args = Array.prototype.slice.call(arguments);
        var request = JSON.parse(args.shift());

        for (var key in request.arguments) {
            if (args.length === 0) {
                console.error("Protocol Error: Invalid number of arguments for '" + request.domain + "Agent." + request.command + "' call. It should have the next arguments '" + JSON.stringify(request.arguments) + "'.");
                return;
            }
            var value = args.shift();
            if (request.arguments[key] && typeof value !== request.arguments[key]) {
                console.error("Protocol Error: Invalid type of argument '" + key + "' for '" + request.domain + "Agent." + request.command + "' call. It should be '" + request.arguments[key] + "' but it is '" + typeof value + "'.");
                return;
            }
            request.arguments[key] = value;
        }

        var callback;
        if (args.length === 1) {
            if (typeof args[0] !== "function" && typeof args[0] !== "undefined") {
                console.error("Protocol Error: Optional callback argument for '" + request.domain + "Agent." + request.command + "' call should be a function but its type is '" + typeof args[0] + "'.");
                return;
            }
            callback = args[0];
        }
        request.seq = this._wrap(callback || function() {});

        if (window.dumpInspectorProtocolMessages)
            console.log("frontend: " + JSON.stringify(request));

        var message = JSON.stringify(request);
        InspectorFrontendHost.sendMessageToBackend(message);
    },

    registerDomainDispatcher: function(domain, dispatcher)
    {
        this._domainDispatchers[domain] = dispatcher;
    },

    dispatch: function(message)
    {
        if (window.dumpInspectorProtocolMessages)
            console.log("backend: " + ((typeof message === "string") ? message : JSON.stringify(message)));

        var messageObject = (typeof message === "string") ? JSON.parse(message) : message;

        var arguments = [];
        if (messageObject.body)
            for (var key in messageObject.body)
                arguments.push(messageObject.body[key]);

        if ("seq" in messageObject) { // just a response for some request
            if (!messageObject.errors)
                this._callbacks[messageObject.seq].apply(null, arguments);
            else
                this.reportProtocolError(messageObject);

            --this._pendingResponsesCount;
            delete this._callbacks[messageObject.seq];

            if (this._scripts && !this._pendingResponsesCount)
                this.runAfterPendingDispatches();

            return;
        }

        if (messageObject.type === "event") {
            if (!(messageObject.domain in this._domainDispatchers)) {
                console.error("Protocol Error: the message is for non-existing domain '" + messageObject.domain + "'");
                return;
            }
            var dispatcher = this._domainDispatchers[messageObject.domain];
            if (!(messageObject.event in dispatcher)) {
                console.error("Protocol Error: Attempted to dispatch an unimplemented method '" + messageObject.domain + "." + messageObject.event + "'");
                return;
            }
            dispatcher[messageObject.event].apply(dispatcher, arguments);
        }
    },

    reportProtocolError: function(messageObject)
    {
        console.error("Protocol Error: InspectorBackend request with seq = " + messageObject.seq + " failed.");
        for (var i = 0; i < messageObject.errors.length; ++i)
            console.error("    " + messageObject.errors[i]);
    },

    runAfterPendingDispatches: function(script)
    {
        if (!this._scripts)
            this._scripts = [];

        if (script)
            this._scripts.push(script);

        if (!this._pendingResponsesCount) {
            var scripts = this._scripts;
            this._scripts = []
            for (var id = 0; id < scripts.length; ++id)
                 scripts[id].call(this);
        }
    }
}

InspectorBackend = new InspectorBackendStub();

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

    my $forwardHeaders = join("\n", sort(map("#include <" . typeTraits($_, "forwardHeader") . ">", grep(typeTraits($_, "forwardHeader"), keys %{$types}))));
    my $forwardDeclarations = join("\n", sort(map("class " . typeTraits($_, "forward") . ";", grep(typeTraits($_, "forward"), keys %{$types}))));
    my $constantDeclarations = join("\n", @{$constants});
    my $methodsDeclarations = join("\n", @{$methods});

    my $headerBody = << "EOF";
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef ${className}_h
#define ${className}_h

${forwardHeaders}

namespace $namespace {

$forwardDeclarations

typedef String ErrorString;

class $className {
public:
$constructor

$constantDeclarations
$methodsDeclarations

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
    push(@sourceContent, "#include <wtf/text/StringConcatenate.h>");
    push(@sourceContent, "#include <wtf/text/CString.h>");
    push(@sourceContent, "");
    push(@sourceContent, "#if ENABLE(INSPECTOR)");
    push(@sourceContent, "");

    my %headers;
    foreach my $type (keys %{$types}) {
        $headers{"#include \"" . typeTraits($type, "header") . "\""} = 1 if !typeTraits($type, "header") eq  "";
    }
    push(@sourceContent, sort keys %headers);
    push(@sourceContent, "");
    push(@sourceContent, "namespace $namespace {");
    push(@sourceContent, "");
    push(@sourceContent, join("\n", @{$constants}));
    push(@sourceContent, "");
    push(@sourceContent, @{$methods});
    push(@sourceContent, "");
    push(@sourceContent, "} // namespace $namespace");
    push(@sourceContent, "");
    push(@sourceContent, "#endif // ENABLE(INSPECTOR)");
    push(@sourceContent, "");
    return @sourceContent;
}

sub typeTraits
{
    my $type = shift;
    my $trait = shift;
    return $typeTransform{$type}->{$trait};
}

sub parameterDocType
{
    my $parameter = shift;
    my $subtype = $parameter->extendedAttributes->{"type"};
    if ($subtype) {
        my $pattern = typeTraits($parameter->type, "DocType");
        return sprintf($pattern, "&lt;$subtype&gt;");
    }

    my $subtypeRef = $parameter->extendedAttributes->{"typeRef"};
    if ($subtypeRef) {
        my $pattern = typeTraits($parameter->type, "DocType");
        return sprintf($pattern, "&lt;<a href='#$subtypeRef'>" . $subtypeRef . "</a>&gt;");
    }

    return "&lt;" . typeTraits($parameter->type, "JSType") . "&gt;";
}

sub parameterDocLine
{
    my $parameter = shift;

    my $result = $parameter->name . ": " . parameterDocType($parameter);
    my $doc = $parameter->extendedAttributes->{"doc"};
    if ($doc) {
        $result = $result . " // " . $doc;
    }
    return $result;
}

sub finish
{
    my $object = shift;

    push(@backendMethodsImpl, generateBackendDispatcher());
    push(@backendMethodsImpl, generateBackendReportProtocolError());
    unshift(@frontendMethodsImpl, generateFrontendConstructorImpl(), "");

    open(my $SOURCE, ">$outputDir/$frontendClassName.cpp") || die "Couldn't open file $outputDir/$frontendClassName.cpp";
    print $SOURCE join("\n", generateSource($frontendClassName, \%frontendTypes, \@frontendConstantDefinitions, \@frontendMethodsImpl));
    close($SOURCE);
    undef($SOURCE);

    open(my $HEADER, ">$outputHeadersDir/$frontendClassName.h") || die "Couldn't open file $outputHeadersDir/$frontendClassName.h";
    print $HEADER generateHeader($frontendClassName, \%frontendTypes, $frontendConstructor, \@frontendConstantDeclarations, \@frontendMethods, join("\n", @frontendFooter));
    close($HEADER);
    undef($HEADER);

    # Make dispatcher methods private on the backend.
    push(@backendConstantDeclarations, "");
    push(@backendConstantDeclarations, "private:");

    foreach my $type (keys %backendTypes) {
        if (typeTraits($type, "JSONType")) {
            push(@backendMethodsImpl, generateArgumentGetters($type));
        }
    }

    push(@backendMethodsImpl, generateBackendMessageParser());
    push(@backendMethodsImpl, "");

    push(@backendConstantDeclarations, "");

    open($SOURCE, ">$outputDir/$backendClassName.cpp") || die "Couldn't open file $outputDir/$backendClassName.cpp";
    print $SOURCE join("\n", generateSource($backendClassName, \%backendTypes, \@backendConstantDefinitions, \@backendMethodsImpl));
    close($SOURCE);
    undef($SOURCE);

    open($HEADER, ">$outputHeadersDir/$backendClassName.h") || die "Couldn't open file $outputHeadersDir/$backendClassName.h";
    print $HEADER join("\n", generateHeader($backendClassName, \%backendTypes, $backendConstructor, \@backendConstantDeclarations, \@backendMethods, $backendFooter));
    close($HEADER);
    undef($HEADER);

    open(my $JS_STUB, ">$outputDir/$backendJSStubName.js") || die "Couldn't open file $outputDir/$backendJSStubName.js";
    print $JS_STUB join("\n", generateBackendStubJS());
    close($JS_STUB);
    undef($JS_STUB);

    open(my $DOCS, ">$outputDir/WebInspectorProtocol.html") || die "Couldn't open file $outputDir/WebInspectorProtocol.html";
    print $DOCS "<ol class='toc' style='list-style: none; padding: 0'>";
    print $DOCS join("\n", @documentationToc);
    print $DOCS "</ol>";
    print $DOCS join("\n", @documentationLines);
    close($DOCS);
    undef($DOCS);
}

1;
