description(
"This test checks that function declarations are treated as statements."
);

function f()
{
    return false;
}

function ifTest()
{
    if (true)
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("ifTest()");

function ifElseTest()
{
    if (false)
        return false;
    else
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("ifElseTest()");

function doWhileTest()
{
    var i = 0;
    do
        function f()
        {
            return true;
        }
    while (i++ < 10)

    return f();
}

shouldBeTrue("doWhileTest()");

function whileTest()
{
    var i = 0;
    while (i++ < 10)
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("whileTest()");

function forTest()
{
    var i;
    for (i = 0; i < 10; ++i)
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("forTest()");

function forVarTest()
{
    for (var i = 0; i < 10; ++i)
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("forVarTest()");

function forInTest()
{
    var a;
    for (a in { field: false })
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("forInTest()");

function forInVarTest()
{
    var a;
    for (var a in { field: false })
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("forInVarTest()");

function forInVarInitTest()
{
    var a;
    for (var a = false in { field: false })
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("forInVarInitTest()");

function withTest()
{
    with ({ })
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("withTest()");

function labelTest()
{
    label:
        function f()
        {
            return true;
        }

    return f();
}

shouldBeTrue("labelTest()");

var successfullyParsed = true;
