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
