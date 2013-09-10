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

    const a, arguments = true;
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

function lexicalArgumentsLiveRead1(a, b, c)
{
    var o = arguments;
    a = 1;
    return lexicalArgumentsLiveRead1.arguments[0];
}
shouldBe("lexicalArgumentsLiveRead1(0, 2, 3)", "1");

function lexicalArgumentsLiveRead2(a, b, c)
{
    var o = arguments;
    b = 2;
    return lexicalArgumentsLiveRead2.arguments[1];
}
shouldBe("lexicalArgumentsLiveRead2(1, 0, 3)", "2");

function lexicalArgumentsLiveRead3(a, b, c)
{
    var o = arguments;
    c = 3;
    return lexicalArgumentsLiveRead3.arguments[2];
}
shouldBe("lexicalArgumentsLiveRead3(1, 2, 0)", "3");

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
