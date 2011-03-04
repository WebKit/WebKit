var initialize_ProtocolTest = function() {

InspectorTest.filterProps = function(something, nondeterministicProps)
{
    if (something instanceof Object)
        for (var prop in something)
            if (prop in nondeterministicProps)
                something[prop] = "<" + typeof something[prop] + ">";
            else
                something[prop] = this.filterProps(something[prop], nondeterministicProps);
    else if (something instanceof Array)
        for (var i = 0; i < something.length; ++i)
            something[i] = this.filterProps(something[i], nondeterministicProps);
    else if (typeof something === "number")
        something = "<number>";
    return something;
};

InspectorTest._dumpTestResult = function(callArguments)
{
    var functionName = callArguments.shift();
    this.filterProps(callArguments, this._nondeterministicProps);
    var expression = JSON.stringify(callArguments);
    expression = expression.slice(1, expression.length - 1).replace(/\"<number>\"/g, "<number>");;
    var sentObject = JSON.parse(this._lastSentTestMessage);
    var receivedObject = (typeof this._lastReceivedMessage === "string") ? JSON.parse(this._lastReceivedMessage) : this._lastReceivedMessage;

    InspectorTest.addResult("-----------------------------------------------------------");
    InspectorTest.addResult(this._agentName + "." + functionName + "(" + expression + ")");
    InspectorTest.addResult("");
    InspectorTest.addResult("request:");
    InspectorTest.addObject(sentObject, this._nondeterministicProps);
    InspectorTest.addResult("");
    InspectorTest.addResult("response:");
    InspectorTest.addObject(receivedObject, this._nondeterministicProps);
    InspectorTest.addResult("");
};

InspectorTest._callback = function(callArguments, result)
{
    this._dumpTestResult(callArguments);
    this._runNextStep(result);
};

InspectorTest._runNextStep = function(result)
{
    var step = ++this._step;
    var nextTest = this._testSuite[step];
    if (nextTest) {
        var nextTestCopy = JSON.parse(JSON.stringify(nextTest));
        nextTest.push(this._callback.bind(this, nextTestCopy));
        var functionName = nextTest.shift();
        this._agentCoverage[functionName] = "checked";
        this._agent[functionName].apply(this._agent, nextTest);
        this._lastSentTestMessage = this._lastSentMessage;
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
    this._nondeterministicProps = {};
    for (var i = 0; i < nondeterministicProps.length; ++i)
        this._nondeterministicProps[nondeterministicProps[i]] = true;
    this._agent = window[agentName];
    this._agentCoverage = {};
    for (var key in this._agent)
        this._agentCoverage[key] = "not checked";
    this._step = -1;

    this._originalDispatch = InspectorBackend.dispatch;
    InspectorBackend.dispatch = function(message) { InspectorTest._lastReceivedMessage = message; InspectorTest._originalDispatch.apply(InspectorBackend, [message]); }

    this._originalSendMessageToBackend = InspectorFrontendHost.sendMessageToBackend;
    InspectorFrontendHost.sendMessageToBackend = function(message) { InspectorTest._lastSentMessage = message; InspectorTest._originalSendMessageToBackend.apply(InspectorFrontendHost, [message]); }

    this._runNextStep();
};

};