/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

TestPage.registerInitializer(function() {

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
    if (!(suite instanceof AsyncTestSuite))
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
                ProtocolTest.expectThat(source === expected.source, "ConsoleMessage type should be '" + expected.source + "'.");
                ProtocolTest.expectThat(level === expected.level, "ConsoleMessage level should be '" + expected.level + "'.");

                if (expected.text)
                    ProtocolTest.expectThat(text === expected.text, "ConsoleMessage text should be '" + expected.text + "'.");

                if (expected.parameters) {
                    ProtocolTest.expectThat(parameters.length === expected.parameters.length, "ConsoleMessage parameters.length === " + expected.parameters.length);
                    for (var i = 0; i < parameters.length; ++i) {
                        var expectedType = expected.parameters[i];
                        ProtocolTest.expectThat(parameters[i].type === expectedType, "ConsoleMessage parameter " + i + " should have type '" + expectedType + "'.");
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

});
