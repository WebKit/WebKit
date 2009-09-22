description('Tests to ensure that Function.apply works correctly for Arrays and arguments.');

function argumentsApply1(a, b, c)
{
    function t(a, b, c)
    {
        return a;
    }
    return t.apply(null, arguments);
}

function argumentsApply2(a, b, c)
{
    function t(a, b, c)
    {
        return b;
    }
    return t.apply(null, arguments);
}

function argumentsApply3(a, b, c)
{
    function t(a, b, c)
    {
        return c;
    }
    return t.apply(null, arguments);
}

function argumentsApplyLength(a, b, c)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    return t.apply(null, arguments);
}
var executedAdditionalArgument = false;
function argumentsApplyExcessArguments(a, b, c)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    return t.apply(null, arguments, executedAdditionalArgument = true);
}

shouldBe("argumentsApply1(1, 2, 3)", "1");
shouldBe("argumentsApply2(1, 2, 3)", "2");
shouldBe("argumentsApply3(1, 2, 3)", "3");
shouldBe("argumentsApplyLength(1, 2, 3)", "3");
shouldBe("argumentsApplyExcessArguments(1, 2, 3)", "3");
shouldBeTrue("executedAdditionalArgument");

function arrayApply1(array)
{
    function t(a, b, c)
    {
        return a;
    }
    return t.apply(null, array);
}

function arrayApply2(array)
{
    function t(a, b, c)
    {
        return b;
    }
    return t.apply(null, array);
}

function arrayApply3(array)
{
    function t(a, b, c)
    {
        return c;
    }
    return t.apply(null, array);
}

function arrayApplyLength(array)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    return t.apply(null, array);
}

shouldBe("arrayApply1([1, 2, 3])", "1");
shouldBe("arrayApply2([1, 2, 3])", "2");
shouldBe("arrayApply3([1, 2, 3])", "3");
shouldBe("arrayApplyLength([1, 2, 3])", "3");


function argumentsApplyDelete1(a, b, c)
{
    function t(a, b, c)
    {
        return a;
    }
    delete arguments[1];
    return t.apply(null, arguments);
}

function argumentsApplyDelete2(a, b, c)
{
    function t(a, b, c)
    {
        return b;
    }
    delete arguments[1];
    return t.apply(null, arguments);
}

function argumentsApplyDelete3(a, b, c)
{
    function t(a, b, c)
    {
        return c;
    }
    delete arguments[1];
    return t.apply(null, arguments);
}

function argumentsApplyDeleteLength(a, b, c)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    delete arguments[1];
    return t.apply(null, arguments);
}

shouldBe("argumentsApplyDelete1(1, 2, 3)", "1");
shouldBe("argumentsApplyDelete2(1, 2, 3)", "undefined");
shouldBe("argumentsApplyDelete3(1, 2, 3)", "3");
shouldBe("argumentsApplyDeleteLength(1, 2, 3)", "3");


function arrayApplyDelete1(array)
{
    function t(a, b, c)
    {
        return a;
    }
    delete array[1];
    return t.apply(null, array);
}

function arrayApplyDelete2(array)
{
    function t(a, b, c)
    {
        return b;
    }
    delete array[1];
    return t.apply(null, array);
}

function arrayApplyDelete3(array)
{
    function t(a, b, c)
    {
        return c;
    }
    delete array[1];
    return t.apply(null, array);
}

function arrayApplyDeleteLength(array)
{
    function t(a, b, c)
    {
        return arguments.length;
    }
    delete array[1];
    return t.apply(null, array);
}

shouldBe("arrayApplyDelete1([1, 2, 3])", "1");
shouldBe("arrayApplyDelete2([1, 2, 3])", "undefined");
shouldBe("arrayApplyDelete3([1, 2, 3])", "3");
shouldBe("arrayApplyDeleteLength([1, 2, 3])", "3");


function argumentsApplyChangeLength1()
{
    function f() {
        return arguments.length;
    };
    arguments.length = 2;
    return f.apply(null, arguments);
}


function argumentsApplyChangeLength2()
{
    function f(a) {
        return arguments.length;
    };
    arguments.length = 2;
    return f.apply(null, arguments);
}


function argumentsApplyChangeLength3()
{
    function f(a, b, c) {
        return arguments.length;
    };
    arguments.length = 2;
    return f.apply(null, arguments);
};

function argumentsApplyChangeLength4()
{
    function f() {
        return arguments.length;
    };
    arguments.length = 0;
    return f.apply(null, arguments);
};

function argumentsApplyChangeLength5()
{
    function f() {
        return arguments.length;
    };
    arguments.length = "Not A Number";
    return f.apply(null, arguments);
}

shouldBe("argumentsApplyChangeLength1(1)", "2");
shouldBe("argumentsApplyChangeLength2(1)", "2");
shouldBe("argumentsApplyChangeLength3(1)", "2");
shouldBe("argumentsApplyChangeLength4(1)", "0");
shouldBe("argumentsApplyChangeLength5(1)", "0");

function arrayApplyChangeLength1()
{
    function f() {
        return arguments.length;
    };
    var array = [];
    array.length = 2;
    return f.apply(null, array);
}

function arrayApplyChangeLength2()
{
    function f(a) {
        return arguments.length;
    };
    var array = [];
    array.length = 2;
    return f.apply(null, array);
}

function arrayApplyChangeLength3()
{
    function f(a, b, c) {
        return arguments.length;
    };
    var array = [];
    array.length = 2;
    return f.apply(null, array);
}

function arrayApplyChangeLength4()
{
    function f() {
        return arguments.length;
    };
    var array = [1];
    array.length = 0;
    return f.apply(null, array);
};

shouldBe("arrayApplyChangeLength1()", "2");
shouldBe("arrayApplyChangeLength2()", "2");
shouldBe("arrayApplyChangeLength3()", "2");
shouldBe("arrayApplyChangeLength4()", "0");

var successfullyParsed = true;
