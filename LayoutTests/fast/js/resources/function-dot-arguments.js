description(
"This test checks corner cases of 'f.arguments'."
);

function assignTest()
{
    function g()
    {
        return assignTest.arguments;
    }

    arguments = true;
    return g();
}
shouldBeTrue("assignTest()");

function assignVarUndefinedTest()
{
    function g()
    {
        return assignVarUndefinedTest.arguments;
    }

    var arguments;
    return g();
}
shouldBeUndefined("assignVarUndefinedTest()");

function assignVarUndefinedTest2()
{
    function g()
    {
        return assignVarUndefinedTest2.arguments;
    }

    var a, arguments;
    return g();
}
shouldBeUndefined("assignVarUndefinedTest2()");

function assignVarInitTest()
{
    function g()
    {
        return assignVarInitTest.arguments;
    }

    var arguments = true;
    return g();
}
shouldBeTrue("assignVarInitTest()");

function assignVarInitTest2()
{
    function g()
    {
        return assignVarInitTest2.arguments;
    }

    var a, arguments = true;
    return g();
}
shouldBeTrue("assignVarInitTest2()");

function assignConstUndefinedTest()
{
    function g()
    {
        return assignConstUndefinedTest.arguments;
    }

    var arguments;
    return g();
}
shouldBeUndefined("assignConstUndefinedTest()");

function assignConstUndefinedTest2()
{
    function g()
    {
        return assignConstUndefinedTest2.arguments;
    }

    var a, arguments;
    return g();
}
shouldBeUndefined("assignConstUndefinedTest2()");

function assignConstInitTest()
{
    function g()
    {
        return assignConstInitTest.arguments;
    }

    const arguments = true;
    return g();
}
shouldBeTrue("assignConstInitTest()");

function assignConstInitTest2()
{
    function g()
    {
        return assignConstInitTest2.arguments;
    }

    const a, arguments = true;
    return g();
}
shouldBeTrue("assignConstInitTest2()");

function assignForInitTest()
{
    function g()
    {
        return assignForInitTest.arguments;
    }

    for (var arguments = true; false;) { }
    return g();
}
shouldBeTrue("assignForInitTest()");

function assignForInitTest2()
{
    function g()
    {
        return assignForInitTest2.arguments;
    }

    for (var a, arguments = true; false;) { }
    return g();
}
shouldBeTrue("assignForInitTest2()");

function assignForInInitTest()
{
    function g()
    {
        return assignForInInitTest.arguments;
    }

    for (arguments = true; false;) { }
    return g();
}
shouldBeTrue("assignForInInitTest()");

function paramInitTest(arguments)
{
    function g()
    {
        return paramInitTest.arguments;
    }

    return g();
}
shouldBeTrue("paramInitTest(true)");

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

var successfullyParsed = true;
