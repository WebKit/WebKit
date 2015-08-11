ProtocolTest.Console = {};

ProtocolTest.Console.sanitizeConsoleMessage = function(messageObject)
{
    function basename(url)
    {
        return url.substring(url.lastIndexOf("/") + 1) || "???";
    }

    var message = messageObject.params.message;
    var obj = {
        source: message.source,
        level: message.level,
        text: message.text,
        location: basename(message.url) + ":" + message.line + ":" + message.column
    };

    if (message.parameters) {
        var params = [];
        for (var i = 0; i < message.parameters.length; ++i) {
            var param = message.parameters[i];
            var o = {type: param.type};
            if (param.subtype)
                o.subtype = param.subtype;
            params.push(o);
        }
        obj.parameters = params;
    }

    return obj;
}

ProtocolTest.Console.addTestCase = function(suite, args)
{
    if (!(suite instanceof ProtocolTest.AsyncTestSuite))
        throw new Error("Console test cases must be added to an async test suite.");

    var {name, description, expression, expected} = args;
    suite.addTestCase({
        name,
        description,
        test: function(resolve, reject) {
            InspectorProtocol.awaitEvent({
                event: "Console.messageAdded",
            })
            .then(function(messageObject) {
                var consoleMessage = messageObject.params.message;
                var {source, level, text, parameters} = consoleMessage;
                ProtocolTest.assert(source === expected.source, "ConsoleMessage type should be '" + expected.source + "'.");
                ProtocolTest.assert(level === expected.level, "ConsoleMessage level should be '" + expected.level + "'.");

                if (expected.text)
                    ProtocolTest.assert(text === expected.text, "ConsoleMessage text should be '" + expected.text + "'.");

                if (expected.parameters) {
                    ProtocolTest.assert(parameters.length === expected.parameters.length, "ConsoleMessage parameters.length === " + expected.parameters.length);
                    for (var i = 0; i < parameters.length; ++i) {
                        var expectedType = expected.parameters[i];
                        ProtocolTest.assert(parameters[i].type === expectedType, "ConsoleMessage parameter " + i + " should have type '" + expectedType + "'.");
                    }
                }

                resolve();
            })
            .catch(reject);

            // Cause a messageAdded event to be generated.
            ProtocolTest.log("Evaluating expression: " + expression);
            InspectorProtocol.sendCommand({
                method: "Runtime.evaluate",
                params: {expression}
            });
        }
    });
}
