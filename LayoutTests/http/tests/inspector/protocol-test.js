var initialize_ProtocolTest = function() {

InspectorTest.filterProps = function(object, nondeterministicProps)
{
    if (object instanceof Object)
        for (var prop in object)
            if (prop in nondeterministicProps)
                object[prop] = "<" + typeof object[prop] + ">";
            else
                object[prop] = this.filterProps(object[prop], nondeterministicProps);
    else if (object instanceof Array)
        for (var i = 0; i < object.length; ++i)
            object[i] = this.filterProps(object[i], nondeterministicProps);
    else if (typeof object === "number")
        object = "<number>";
    else if (typeof object === "string" && object.indexOf("\"id\"") != -1)
        object = "<string>";
    return object;
};

InspectorTest._dumpEvent = function()
{
    var args = Array.prototype.slice.call(arguments);
    var eventName = args.shift();
    InspectorTest._agentCoverage[eventName] = "checked";
    InspectorTest.addResult("event " + InspectorTest._agentName + "." + eventName);
    InspectorTest.addObject(InspectorTest._lastReceivedMessage, InspectorTest._customFormatters);
    InspectorTest.addResult("");

    var originalEventHandler = args.shift();
    originalEventHandler.apply(this, args);
};


InspectorTest._dumpCallArguments = function(callArguments)
{
    var callArgumentsCopy = JSON.parse(JSON.stringify(callArguments));
    var agentName = callArgumentsCopy.shift();
    var functionName = callArgumentsCopy.shift();
    this.filterProps(callArgumentsCopy, this._customFormatters);
    var expression = JSON.stringify(callArgumentsCopy);
    expression = expression.slice(1, expression.length - 1).replace(/\"<number>\"/g, "<number>").replace(/\"<string>\"/g, "<string>");

    InspectorTest.addResult("-----------------------------------------------------------");
    InspectorTest.addResult(agentName + "." + functionName + "(" + expression + ")");
    InspectorTest.addResult("");
};

InspectorTest._callback = function(result)
{
    InspectorTest.addResult("response:");
    InspectorTest.addObject(InspectorTest._lastReceivedMessage, InspectorTest._customFormatters);
    InspectorTest.addResult("");
    InspectorTest._runNextTest();
};

InspectorTest._runNextTest = function()
{
    var step = ++this._step;
    var nextTest = this._testSuite[step];
    if (nextTest) {
        InspectorTest._dumpCallArguments(nextTest);

        nextTest.push(this._callback.bind(this));

        var agentName = nextTest.shift();
        var functionName = nextTest.shift();
        window[agentName][functionName].apply(window[agentName], nextTest);

        var lastSentMessage = InspectorTest._lastSentMessage; // This is because the next call will override _lastSentMessage.
        InspectorTest.addResult("request:");
        InspectorTest.addObject(lastSentMessage, InspectorTest._customFormatters);
        InspectorTest.addResult("");

        if (agentName === this._agentName)
            this._agentCoverage[functionName] = "checked";
    }
    else {
        InspectorTest.addResult("===========================================================");
        InspectorTest.addResult("Coverage for " + this._agentName);
        InspectorTest.addObject(this._agentCoverage);
        InspectorBackend.dispatch = this._originalDispatch;
        InspectorFrontendHost.sendMessageToBackend = this._originalSendMessageToBackend;
        this.completeTest();
    }
};

InspectorTest.runProtocolTestSuite = function(agentName, testSuite, nondeterministicProps)
{
    this._agentName = agentName;
    this._testSuite = testSuite;
    this._customFormatters = {};
    for (var i = 0; i < nondeterministicProps.length; ++i)
        this._customFormatters[nondeterministicProps[i]] = "formatAsTypeName";
    var agent = window[agentName];

    this._agentCoverage = {};
    for (var key in agent)
        this._agentCoverage[key] = "not checked";

    var domain = agentName.replace(/Agent$/,"");
    var domainMessagesHandler = InspectorBackend._domainDispatchers[domain];
    for (var eventName in domainMessagesHandler) {
        if (typeof domainMessagesHandler[eventName] !== "function")
            continue;
        this._agentCoverage[eventName] = "not checked";
        domainMessagesHandler[eventName] = InspectorTest._dumpEvent.bind(domainMessagesHandler, eventName, domainMessagesHandler[eventName]);
    }

    this._originalDispatch = InspectorBackend.dispatch;
    InspectorBackend.dispatch = function(message)
    {
        InspectorTest._lastReceivedMessage = (typeof message === "string") ? JSON.parse(message) : message;
        InspectorTest._originalDispatch.apply(InspectorBackend, [message]);
    }

    this._originalSendMessageToBackend = InspectorFrontendHost.sendMessageToBackend;
    InspectorFrontendHost.sendMessageToBackend = function(message)
    {
        InspectorTest._lastSentMessage = JSON.parse(message);
        InspectorTest._originalSendMessageToBackend.apply(InspectorFrontendHost, [message]);
    }

    this._step = -1;

    this._runNextTest();
};

};
