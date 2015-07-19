description(
"This test checks basic and corner cases of 'f.arguments'."
);

function basicTest(args)
{
    return basicTest.arguments;
}
shouldBe("basicTest('one')[0]", "'one'");
shouldBeUndefined("basicTest('one')[2]");
shouldBe("basicTest('one', 'two', 'three')[1]", "'two'");

function lengthTest()
{
    return "" + lengthTest.arguments.length;
}
shouldBe("lengthTest()", "'0'");
shouldBe("lengthTest('From', '%E5%8C%97%E4%BA%AC', 360, '/', 'webkit.org')", "'5'");

function assignTest()
{
    function g()
    {
        return assignTest.arguments;
    }

    arguments = true;
    return g();
}
shouldBe("assignTest().toString()", "'[object Arguments]'");

function assignVarUndefinedTest()
{
    function g()
    {
        return assignVarUndefinedTest.arguments;
    }

    var arguments;
    return g();
}
shouldBe("assignVarUndefinedTest().toString()", "'[object Arguments]'");

function assignVarUndefinedTest2()
{
    function g()
    {
        return assignVarUndefinedTest2.arguments;
    }

    var a, arguments;
    return g();
}
shouldBe("assignVarUndefinedTest2().toString()", "'[object Arguments]'");

function assignVarInitTest()
{
    function g()
    {
        return assignVarInitTest.arguments;
    }

    var arguments = true;
    return g();
}
shouldBe("assignVarInitTest().toString()", "'[object Arguments]'");

function assignVarInitTest2()
{
    function g()
    {
        return assignVarInitTest2.arguments;
    }

    var a, arguments = true;
    return g();
}
shouldBe("assignVarInitTest2().toString()", "'[object Arguments]'");

function assignConstUndefinedTest()
{
    function g()
    {
        return assignConstUndefinedTest.arguments;
    }

    var arguments;
    return g();
}
shouldBe("assignConstUndefinedTest().toString()", "'[object Arguments]'");

function assignConstUndefinedTest2()
{
    function g()
    {
        return assignConstUndefinedTest2.arguments;
    }

    var a, arguments;
    return g();
}
shouldBe("assignConstUndefinedTest2().toString()", "'[object Arguments]'");

function assignConstInitTest()
{
    function g()
    {
        return assignConstInitTest.arguments;
    }

    const arguments = true;
    return g();
}
shouldBe("assignConstInitTest().toString()", "'[object Arguments]'");

function assignConstInitTest2()
{
    function g()
    {
        return assignConstInitTest2.arguments;
    }

    const a = 5, arguments = true;
    return g();
}
shouldBe("assignConstInitTest2().toString()", "'[object Arguments]'");

function assignForInitTest()
{
    function g()
    {
        return assignForInitTest.arguments;
    }

    for (var arguments = true; false;) { }
    return g();
}
shouldBe("assignForInitTest().toString()", "'[object Arguments]'");

function assignForInitTest2()
{
    function g()
    {
        return assignForInitTest2.arguments;
    }

    for (var a, arguments = true; false;) { }
    return g();
}
shouldBe("assignForInitTest2().toString()", "'[object Arguments]'");

function assignForInInitTest()
{
    function g()
    {
        return assignForInInitTest.arguments;
    }

    for (arguments = true; false;) { }
    return g();
}
shouldBe("assignForInInitTest().toString()", "'[object Arguments]'");

function paramInitTest(arguments)
{
    return paramInitTestCheckArguments();
}
function paramInitTestCheckArguments()
{
    return paramInitTest.arguments;
}
shouldBe("paramInitTest(true).toString()", "'[object Arguments]'");

var paramFunctionConstructorInitTest = Function("arguments", "return paramFunctionConstructorInitTestCheckArguments();");
function paramFunctionConstructorInitTestCheckArguments()
{
    return paramFunctionConstructorInitTest.arguments;
}
shouldBe("paramFunctionConstructorInitTest(true).toString()", "'[object Arguments]'");

function tearOffTest()
{
    function g()
    {
        var a = 1;
        return arguments;
    }

    var b = 2;
    var arguments = g(true);
    return arguments;
}
shouldBeTrue("tearOffTest()[0]");

function tearOffTest2()
{
    function g(a)
    {
        var arguments = a;
        var b = 2;
        return arguments;
    }

    var c = 3;
    return g(arguments);
}
shouldBeTrue("tearOffTest2(true)[0]");


// Some utility functions/
function arrayify(args) {
    if (typeof args != "object")
        return args;
    if (typeof args.length == "undefined" || typeof args.callee == "undefined")
        return args;
    return Array.prototype.slice.call(args);
}

function indirectCall(callee)
{
    return callee();
}

// Test reading from caller.arguments from an inner function.
function tearOffTest3(a, b, c, d)
{
    a = 10;
    function inner()
    {
        return tearOffTest3.arguments;
    }

    return arrayify(inner());
}
shouldBe("tearOffTest3(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest3a(a, b, c, d)
{
    var x = 42;
    a = 10;
    function inner()
    {
        return tearOffTest3a.arguments;
    }

    if (d) {
        // Force a lexicalEnvironment to be created in the outer function.
        return function() { return x; }
    } else {
        return arrayify(inner());
    }
}
shouldBe("tearOffTest3a(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest3b(a, b, c, d)
{
    a = 10;
    function inner()
    {
        var capture = a; // Capture an arg from the outer function.
        return tearOffTest3b.arguments;
    }

    return arrayify(inner());
}
shouldBe("tearOffTest3b(1, 2, 3, false)", "[1, 2, 3, false]");


function tearOffTest3c(a, b, c, d)
{
    a = 10;
    function inner()
    {
        var capture = a; // Capture an arg from the outer function.
        return tearOffTest3c.arguments;
    }

    return arrayify(indirectCall(inner));
}
shouldBe("tearOffTest3c(1, 2, 3, false)", "[1, 2, 3, false]");


// Test reading from caller.arguments from an external function.
function tearOffTest4External()
{
    return tearOffTest4.arguments;
}
function tearOffTest4(a, b, c, d)
{
    a = 10;
    return arrayify(tearOffTest4External());
}
shouldBe("tearOffTest4(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest4aExternal()
{
    return tearOffTest4a.arguments;
}
function tearOffTest4a(a, b, c, d)
{
    var x = 42;
    a = 10;

    if (d) {
        // Force a lexicalEnvironment to be created in the outer function.
        return function() { return x; }
    } else {
        return arrayify(tearOffTest4aExternal());
    }
}
shouldBe("tearOffTest4a(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest4bExternal()
{
    return tearOffTest4b.arguments;
}
function tearOffTest4b(a, b, c, d)
{
    a = 10;
    function inner()
    {
        var capture = a; // Capture an arg from the outer function.
        return capture;
    }

    return arrayify(tearOffTest4bExternal());
}
shouldBe("tearOffTest4b(1, 2, 3, false)", "[1, 2, 3, false]");


function tearOffTest4cExternal()
{
    return tearOffTest4c.arguments;
}
function tearOffTest4c(a, b, c, d)
{
    a = 10;
    function inner()
    {
        var capture = a; // Capture an arg from the outer function.
        return tearOffTest4c.arguments;
    }

    return arrayify(indirectCall(tearOffTest4cExternal));
}
shouldBe("tearOffTest4c(1, 2, 3, false)", "[1, 2, 3, false]");


// Test reading from caller.arguments which have Deleted slow data from an inner function.
function tearOffTest5(a, b, c, d)
{
    a = 10;
    delete arguments[0];
    function inner()
    {
        return tearOffTest5.arguments;
    }

    return arrayify(inner());
}
shouldBe("tearOffTest5(1, 2, 3, false)", "[1, 2, 3, false]");


function tearOffTest5a(a, b, c, d)
{
    var x = 42;
    a = 10;
    delete arguments[0];
    function inner()
    {
        return tearOffTest5a.arguments;
    }

    if (d) {
        // Force a lexicalEnvironment to be created in the outer function.
        return function() { return x; }
    } else {
        return arrayify(inner());
    }
}
shouldBe("tearOffTest5a(1, 2, 3, false)", "[1, 2, 3, false]");


function tearOffTest5b(a, b, c, d)
{
    a = 10;
    delete arguments[0];
    function inner()
    {
        var capture = a; // Capture an arg from the outer function.
        return tearOffTest5b.arguments;
    }

    return arrayify(inner());
}
shouldBe("tearOffTest5b(1, 2, 3, false)", "[1, 2, 3, false]");


function tearOffTest5c(a, b, c, d)
{
    a = 10;
    delete arguments[0];
    function inner()
    {
        var capture = a; // Capture an arg from the outer function.
        return tearOffTest5c.arguments;
    }

    return arrayify(indirectCall(inner));
}
shouldBe("tearOffTest5c(1, 2, 3, false)", "[1, 2, 3, false]");


// Test reading from caller.arguments which have Deleted slow data from an external function.
function tearOffTest6External()
{
    return tearOffTest6.arguments;
}
function tearOffTest6(a, b, c, d)
{
    a = 10;
    delete arguments[0];
    return arrayify(tearOffTest6External());
}
shouldBe("tearOffTest6(1, 2, 3, false)", "[1, 2, 3, false]");


function tearOffTest6aExternal()
{
    return tearOffTest6a.arguments;
}
function tearOffTest6a(a, b, c, d)
{
    var x = 42;
    a = 10;
    delete arguments[0];

    if (d) {
        // Force a lexicalEnvironment to be created in the outer function.
        return function() { return x; }
    } else {
        return arrayify(tearOffTest6aExternal());
    }
}
shouldBe("tearOffTest6a(1, 2, 3, false)", "[1, 2, 3, false]");


function tearOffTest6bExternal()
{
    return tearOffTest6b.arguments;
}
function tearOffTest6b(a, b, c, d)
{
    a = 10;
    delete arguments[0];
    function inner()
    {
        var capture = a; // Capture an arg from the outer function.
        return capture;
    }

    return arrayify(tearOffTest6bExternal());
}
shouldBe("tearOffTest6b(1, 2, 3, false)", "[1, 2, 3, false]");


function tearOffTest6cExternal()
{
    return tearOffTest6c.arguments;
}
function tearOffTest6c(a, b, c, d)
{
    a = 10;
    delete arguments[0];
    function inner()
    {
        var capture = a; // Capture an arg from the outer function.
        return tearOffTest6c.arguments;
    }

    return arrayify(indirectCall(tearOffTest6cExternal));
}
shouldBe("tearOffTest6c(1, 2, 3, false)", "[1, 2, 3, false]");


// Test writing to caller.arguments from an inner function.
function tearOffTest7(a, b, c, d)
{
    a = 10;
    (function inner() {
        tearOffTest7.arguments[0] = 100;
    })();

    return arrayify(arguments);
}
shouldBe("tearOffTest7(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest7a(a, b, c, d)
{
    var x = 42;
    a = 10;

    if (d) {
        // Force a lexicalEnvironment to be created in the outer function.
        return function() { return x; }
    } else {
        (function inner() {
            tearOffTest7a.arguments[0] = 100;
        }) ();

        return arrayify(arguments);
    }
}
shouldBe("tearOffTest7a(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest7b(a, b, c, d)
{
    a = 10;
    (function inner() {
        var capture = a; // Capture an arg from the outer function.
        tearOffTest7b.arguments[0] = 100;
    })();

    return arrayify(arguments);
}
shouldBe("tearOffTest7b(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest7c(a, b, c, d)
{
    a = 10;
    function inner() {
        var capture = a; // Capture an arg from the outer function.
        tearOffTest7c.arguments[0] = 100;
    }
    indirectCall(inner);
    return arrayify(arguments);
}
shouldBe("tearOffTest7c(1, 2, 3, false)", "[10, 2, 3, false]");


// Test writing to caller.arguments from an external function.
function tearOffTest8External() {
    tearOffTest8.arguments[0] = 100;
}
function tearOffTest8(a, b, c, d)
{
    a = 10;
    tearOffTest8External();

    return arrayify(arguments);
}
shouldBe("tearOffTest8(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest8aExternal() {
    tearOffTest8a.arguments[0] = 100;
}
function tearOffTest8a(a, b, c, d)
{
    var x = 42;
    a = 10;

    if (d) {
        // Force a lexicalEnvironment to be created in the outer function.
        return function() { return x; }
    } else {
        tearOffTest8aExternal();
        return arrayify(arguments);
    }
}
shouldBe("tearOffTest8a(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest8bExternal() {
    tearOffTest8b.arguments[0] = 100;
}
function tearOffTest8b(a, b, c, d)
{
    a = 10;
    function inner() {
        var capture = a; // Capture an arg from the outer function.
    }
    tearOffTest8bExternal();

    return arrayify(arguments);
}
shouldBe("tearOffTest8b(1, 2, 3, false)", "[10, 2, 3, false]");


function tearOffTest8cExternal() {
    tearOffTest8c.arguments[0] = 100;
}
function tearOffTest8c(a, b, c, d)
{
    a = 10;
    function inner() {
        var capture = a; // Capture an arg from the outer function.
    }
    indirectCall(tearOffTest8cExternal);
    return arrayify(arguments);
}
shouldBe("tearOffTest8c(1, 2, 3, false)", "[10, 2, 3, false]");


// Test deleting an arg in caller.arguments from an inner function.
function tearOffTest9(a, b, c, d)
{
    a = 10;
    delete arguments[0];
    (function inner() {
        delete tearOffTest9.arguments[1];
    })();

    return arrayify(arguments);
}
shouldBe("tearOffTest9(1, 2, 3, false)", "[undefined, 2, 3, false]");


function tearOffTest9a(a, b, c, d)
{
    var x = 42;
    delete arguments[0];

    if (d) {
        // Force a lexicalEnvironment to be created in the outer function.
        return function() { return x; }
    } else {
        (function inner() {
            delete tearOffTest9a.arguments[1];
        }) ();

        return arrayify(arguments);
    }
}
shouldBe("tearOffTest9a(1, 2, 3, false)", "[undefined, 2, 3, false]");


function tearOffTest9b(a, b, c, d)
{
    delete arguments[0];
    (function inner() {
        var capture = a; // Capture an arg from the outer function.
        delete tearOffTest9b.arguments[1];
    })();

    return arrayify(arguments);
}
shouldBe("tearOffTest9b(1, 2, 3, false)", "[undefined, 2, 3, false]");


function tearOffTest9c(a, b, c, d)
{
    delete arguments[0];
    function inner() {
        var capture = a; // Capture an arg from the outer function.
        delete tearOffTest9c.arguments[1];
    }
    indirectCall(inner);
    return arrayify(arguments);
}
shouldBe("tearOffTest9c(1, 2, 3, false)", "[undefined, 2, 3, false]");


// Test deleting a arg in caller.arguments from an external function.

function tearOffTest10External() {
    delete tearOffTest10.arguments[1];
}
function tearOffTest10(a, b, c, d)
{
    delete arguments[0];
    tearOffTest10External();

    return arrayify(arguments);
}
shouldBe("tearOffTest10(1, 2, 3, false)", "[undefined, 2, 3, false]");


function tearOffTest10aExternal() {
    delete tearOffTest10a.arguments[1];
}
function tearOffTest10a(a, b, c, d)
{
    var x = 42;
    delete arguments[0];

    if (d) {
        // Force a lexicalEnvironment to be created in the outer function.
        return function() { return x; }
    } else {
        tearOffTest10aExternal();
        return arrayify(arguments);
    }
}
shouldBe("tearOffTest10a(1, 2, 3, false)", "[undefined, 2, 3, false]");


function tearOffTest10bExternal() {
    delete tearOffTest10b.arguments[1];
}
function tearOffTest10b(a, b, c, d)
{
    delete arguments[0];
    function inner() {
        var capture = a; // Capture an arg from the outer function.
    }
    tearOffTest10bExternal();

    return arrayify(arguments);
}
shouldBe("tearOffTest10b(1, 2, 3, false)", "[undefined, 2, 3, false]");


function tearOffTest10cExternal() {
    delete tearOffTest10c.arguments[1];
}
function tearOffTest10c(a, b, c, d)
{
    delete arguments[0];
    function inner() {
        var capture = a; // Capture an arg from the outer function.
    }
    indirectCall(tearOffTest10cExternal);
    return arrayify(arguments);
}
shouldBe("tearOffTest10c(1, 2, 3, false)", "[undefined, 2, 3, false]");


function lexicalArgumentsLiveRead1(a, b, c)
{
    var o = arguments;
    a = 1;
    return lexicalArgumentsLiveRead1.arguments[0];
}
shouldBe("lexicalArgumentsLiveRead1(0, 2, 3)", "0");

function lexicalArgumentsLiveRead2(a, b, c)
{
    var o = arguments;
    b = 2;
    return lexicalArgumentsLiveRead2.arguments[1];
}
shouldBe("lexicalArgumentsLiveRead2(1, 0, 3)", "0");

function lexicalArgumentsLiveRead3(a, b, c)
{
    var o = arguments;
    c = 3;
    return lexicalArgumentsLiveRead3.arguments[2];
}
shouldBe("lexicalArgumentsLiveRead3(1, 2, 0)", "0");

function lexicalArgumentsLiveWrite1(a, b, c)
{
    var o = arguments;
    lexicalArgumentsLiveWrite1.arguments[0] = 1;
    return a;
}
shouldBe("lexicalArgumentsLiveWrite1(0, 2, 3)", "0");

function lexicalArgumentsLiveWrite2(a, b, c)
{
    var o = arguments;
    lexicalArgumentsLiveWrite2.arguments[1] = 2;
    return b;
}
shouldBe("lexicalArgumentsLiveWrite2(1, 0, 3)", "0");

function lexicalArgumentsLiveWrite3(a, b, c)
{
    var o = arguments;
    lexicalArgumentsLiveWrite3.arguments[2] = 3;
    return c;
}
shouldBe("lexicalArgumentsLiveWrite3(1, 2, 0)", "0");

function argumentsNotLiveRead1(a, b, c)
{
    var o = argumentsNotLiveRead1.arguments;
    a = 1;
    return o[0];
}
shouldBe("argumentsNotLiveRead1(0, 2, 3)", "0");

function argumentsNotLiveRead2(a, b, c)
{
    var o = argumentsNotLiveRead2.arguments;
    b = 2;
    return o[1];
}
shouldBe("argumentsNotLiveRead2(1, 0, 3)", "0");

function argumentsNotLiveRead3(a, b, c)
{
    var o = argumentsNotLiveRead3.arguments;
    c = 3;
    return o[2];
}
shouldBe("argumentsNotLiveRead3(1, 2, 0)", "0");

function argumentsNotLiveWrite1(a, b, c)
{
    argumentsNotLiveWrite1.arguments[0] = 1;
    return a;
}
shouldBe("argumentsNotLiveWrite1(0, 2, 3)", "0");

function argumentsNotLiveWrite2(a, b, c)
{
    argumentsNotLiveWrite2.arguments[1] = 2;
    return b;
}
shouldBe("argumentsNotLiveWrite2(1, 0, 3)", "0");

function argumentsNotLiveWrite3(a, b, c)
{
    argumentsNotLiveWrite3.arguments[2] = 3;
    return c;
}
shouldBe("argumentsNotLiveWrite3(1, 2, 0)", "0");

function argumentsIdentity()
{
    return argumentsIdentity.arguments != argumentsIdentity.arguments;
}
shouldBeTrue("argumentsIdentity()");

function overwroteArgumentsInDynamicScope1() {
    eval("arguments = true"); 
    return arguments;
}

function overwroteArgumentsInDynamicScope2() {
    arguments = true;
    return eval("arguments");
}

function overwroteArgumentsInDynamicScope3() {
    eval("arguments = true"); 
    return overwroteArgumentsInDynamicScope3.arguments;
}
shouldBeTrue("overwroteArgumentsInDynamicScope1()");
shouldBeTrue("overwroteArgumentsInDynamicScope2()");
shouldBe("overwroteArgumentsInDynamicScope3().toString()", "'[object Arguments]'");
