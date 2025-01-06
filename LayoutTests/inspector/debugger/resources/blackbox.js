function createScripts(id) {
    eval(
`
window.${id}_Inner = function ${id}_Inner(x) {
    return x + 42;
};
//# sourceURL=${id}_Inner.js
`
    );

    eval(
`
window.${id}_Middle = function ${id}_Middle(x) {
    return ${id}_Inner(x);
};
//# sourceURL=${id}_Middle.js
`
    );

    eval(
`
window.${id}_Outer = function ${id}_Outer(x) {
    return ${id}_Middle(x);
};
//# sourceURL=${id}_Outer.js
`
    );

    eval(
`
window.${id}_Multiple = function ${id}_Multiple(x) {
    let y = x * 2;
    let z = y * 2;
    return z * 2;
};
//# sourceURL=${id}_Multiple.js
`
    );
}

TestPage.registerInitializer(() => {
    ProtocolTest.Blackbox = {};

    ProtocolTest.Blackbox.resumeCallback = null;

    let sourceURLRegExpQueries = new Map;
    let pausedFunctionCount = {};

    InspectorProtocol.sendCommand("Debugger.enable", {});
    InspectorProtocol.sendCommand("Debugger.setBreakpointsActive", {active: true});

    InspectorProtocol.eventHandler["Debugger.scriptParsed"] = function(message) {
        let sourceURL = message.params.sourceURL;
        for (let [regExp, callback] of sourceURLRegExpQueries) {
            if (regExp.test(sourceURL)) {
                sourceURLRegExpQueries.delete(regExp);
                callback(sourceURL);
            }
        };
    };

    InspectorProtocol.eventHandler["Debugger.paused"] = function(message) {
        ProtocolTest.newline();

        let topCallFrame = message.params.callFrames[0];
        let functionName = topCallFrame.functionName;
        if (functionName === "global code") {
            ProtocolTest.log("Resuming...");
            InspectorProtocol.sendCommand(`Debugger.resume`, {}, InspectorProtocol.checkForError);
            return;
        }

        ProtocolTest.log(`PAUSED: '${message.params.reason}' at '${functionName}:${topCallFrame.location.lineNumber}:${topCallFrame.location.columnNumber}'.`);
        if (message.params.data)
            ProtocolTest.json(message.params.data);

        pausedFunctionCount[functionName] = (pausedFunctionCount[functionName] || 0) + 1;

        ProtocolTest.log("Stepping over...");
        InspectorProtocol.sendCommand(`Debugger.stepOver`, {}, InspectorProtocol.checkForError);
    };

    InspectorProtocol.eventHandler["Debugger.resumed"] = function(message) {
        ProtocolTest.pass("Resumed.");

        ProtocolTest.Blackbox.resumeCallback?.();
        ProtocolTest.Blackbox.resumeCallback = null;
    };

    ProtocolTest.Blackbox.setBlackbox = async function(url, options = {}) {
        if (!options.caseSensitive)
            url = url.toLowerCase();

        let logPrefix = `Blackboxing ${options.caseSensitive ? "(case sensitive) " : ""}${options.isRegex ? "(regex) " : ""}'${url}'`;
        if (options.sourceRanges) {
            for (let i = 0; i < options.sourceRanges.length; i += 4) {
                 let startLine = options.sourceRanges[i];
                 let startColumn = options.sourceRanges[i + 1];
                 let endLine = options.sourceRanges[i + 2];
                 let endColumn = options.sourceRanges[i + 3];
                 ProtocolTest.log(`${logPrefix} ${startLine}:${startColumn}-${endLine}:${endColumn}...`);
            }
        } else
            ProtocolTest.log(logPrefix + "...");
        await InspectorProtocol.awaitCommand({
            method: "Debugger.setShouldBlackboxURL",
            params: {url, shouldBlackbox: true, ...options},
        });
    };

    ProtocolTest.Blackbox.setBreakpoint = async function(url, lineNumber) {
        ProtocolTest.log(`Setting breakpoint in '${url}'...`);
        await InspectorProtocol.awaitCommand({
            method: "Debugger.setBreakpointByUrl",
            params: {url, lineNumber},
        });
    };

    ProtocolTest.Blackbox.listenForSourceParsed = async function(sourceURLRegExp) {
        return new Promise((resolve, reject) => {
            sourceURLRegExpQueries.set(sourceURLRegExp, resolve);
        });
    };

    ProtocolTest.Blackbox.evaluate = async function(expression) {
        ProtocolTest.log(`Evaluating '${expression}'...`);
        return InspectorProtocol.awaitCommand({
            method: "Runtime.evaluate",
            params: {expression},
        });
    };

    ProtocolTest.Blackbox.pauseCountForFunction = function(functionName) {
        return pausedFunctionCount[functionName] || 0;
    };
});
