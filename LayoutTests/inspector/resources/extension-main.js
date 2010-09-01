function fetchTests()
{
    function callback(result)
    {
         window.eval(result.value);
         runTests();
    }
    webInspector.inspectedWindow.evaluate("extensionFunctions()", callback);
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
    output("All tests done.", function() {
        top.postMessage("extension-tests-done","*");
    });
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
