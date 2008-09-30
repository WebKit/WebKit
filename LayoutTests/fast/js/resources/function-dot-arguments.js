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

var successfullyParsed = true;
