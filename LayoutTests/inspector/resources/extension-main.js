function fetchTests()
{
    function callback(result)
    {
         var functions = JSON.parse(result.value);
         window.eval(functions);
         runTests();
    }
    webInspector.inspectedWindow.evaluate("extensionFunctions()", callback);
}

function runTests()
{
    log("Running tests...");
    var tests = [];
    for (var symbol in this) {
        if (/^extension_test/.exec(symbol) && typeof this[symbol] === "function")
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
    log("All tests done.");
    webInspector.inspectedWindow.evaluate("done();");
}

function runTest(test, name)
{
    log("RUNNING TEST: " + name);
    try {
        test();
    } catch (e) {
        log(name + ": " + e);
    }
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

// dumpObject() needs output(), so implement it via log().

function output(msg)
{
    log(msg);
}

fetchTests();
