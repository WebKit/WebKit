function onError(event)
{
    window.removeEventListener("error", onError);
    output("Uncaught exception in extension context: " + event.message + " [" + event.filename + ":" + event.lineno + "]");
    top.postMessage({ command: "extension-tests-done" }, "*");
}

window.addEventListener("error", onError);

function fetchTests()
{
    function callback(result)
    {
         window.eval(result);
         runTests();
    }
    webInspector.inspectedWindow.eval("extensionFunctions()", callback);
}

function runTests()
{
    output("Running tests...");
    var tests = [];
    for (var symbol in this) {
        if (/^extension_test/.test(symbol) && typeof this[symbol] === "function")
            tests.push(symbol);
    }
    tests = tests.sort();
    var testChain = onTestsDone;
    for (var test = tests.pop(); test; test = tests.pop())
        testChain = bind(runTest, this, bind(this[test], this, testChain), test);
    testChain();
}

function onTestsDone()
{
    output("All tests done.");
    top.postMessage({ command: "extension-tests-done" }, "*");
}

function runTest(test, name)
{
    output("RUNNING TEST: " + name);
    try {
        test();
    } catch (e) {
        output(name + ": " + e);
    }
}

function dispatchOnFrontend(message, callback)
{
    function callbackWrapper()
    {
        channel.port1.removeEventListener("message", callbackWrapper, false);
        callback();
    }
    var channel = new MessageChannel();
    channel.port1.start();
    if (callback)
        channel.port1.addEventListener("message", callbackWrapper, false);
    top.postMessage(message, [ channel.port2 ], "*");
}

function callbackAndNextTest(callback, nextTest)
{
    function callbackWrapper()
    {
        callback.apply(this, arguments);
        nextTest.call(this);
    }
    return callbackWrapper;
}

function bind(func, thisObject)
{
    var args = Array.prototype.slice.call(arguments, 2);
    return function() { return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments, 0))); };
}

fetchTests();
