description(
"This test checks that using the typeof operator on a JavaScript value and comparing it to a constant string works as expected."
);

function isUndefined(a)
{
    return typeof a == "undefined";
}

shouldBeTrue("isUndefined(undefined)");
shouldBeFalse("isUndefined(1)");

function isUndefinedStrict(a)
{
    return typeof a === "undefined";
}

shouldBeTrue("isUndefinedStrict(undefined)");
shouldBeFalse("isUndefinedStrict(1)");

function isBoolean(a)
{
    return typeof a == "boolean";
}

shouldBeTrue("isBoolean(true)");
shouldBeTrue("isBoolean(false)");
shouldBeFalse("isBoolean(1)");

function isBooleanStrict(a)
{
    return typeof a === "boolean";
}

shouldBeTrue("isBooleanStrict(true)");
shouldBeTrue("isBooleanStrict(false)");
shouldBeFalse("isBooleanStrict(1)");

function isNumber(a)
{
    return typeof a == "number";
}

shouldBeTrue("isNumber(1)");
shouldBeFalse("isNumber(undefined)");

function isNumberStrict(a)
{
    return typeof a === "number";
}

shouldBeTrue("isNumberStrict(1)");
shouldBeFalse("isNumberStrict(undefined)");

function isString(a)
{
    return typeof a == "string";
}

shouldBeTrue("isString('string')");
shouldBeFalse("isString(1)");

function isStringStrict(a)
{
    return typeof a === "string";
}

shouldBeTrue("isStringStrict('string')");
shouldBeFalse("isStringStrict(1)");

function isObject(a)
{
    return typeof a == "object";
}

shouldBeTrue("isObject({ })");
shouldBeFalse("isObject(1)");

function isObjectStrict(a)
{
    return typeof a === "object";
}

shouldBeTrue("isObjectStrict({ })");
shouldBeFalse("isObjectStrict(1)");

function isFunction(a)
{
    return typeof a == "function";
}

shouldBeTrue("isFunction(function () { })");
shouldBeFalse("isFunction(1)");

function isFunctionStrict(a)
{
    return typeof a === "function";
}

shouldBeTrue("isFunctionStrict(function () { })");
shouldBeFalse("isFunctionStrict(1)");

function complexIsUndefinedTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o == "undefined") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsUndefinedTest()", "'PASS'");

function complexIsBooleanTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o == "boolean") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsBooleanTest()", "'PASS'");

function complexIsNumberTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o == "number") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsNumberTest()", "'PASS'");

function complexIsStringTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o == "string") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsStringTest()", "'PASS'");

function complexIsObjectTest()
{
    var a = ["text", 0];
    function replace_formats() {
        var o = function () { };
        if (typeof o == "string") {
        } else if (typeof o == "object") {
        } else if (typeof o == "function" && typeof a[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsObjectTest()", "'PASS'");

function complexIsFunctionTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o == "function") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsFunctionTest()", "'PASS'");

function complexIsUndefinedStrictTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o === "undefined") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsUndefinedStrictTest()", "'PASS'");

function complexIsBooleanStrictTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o === "boolean") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsBooleanStrictTest()", "'PASS'");

function complexIsNumberStrictTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o === "number") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsNumberStrictTest()", "'PASS'");

function complexIsStringStrictTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o === "string") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsStringStrictTest()", "'PASS'");

function complexIsObjectStrictTest()
{
    var a = ["text", 0];
    function replace_formats() {
        var o = function () { };
        if (typeof o == "string") {
        } else if (typeof o === "object") {
        } else if (typeof o == "function" && typeof a[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsObjectStrictTest()", "'PASS'");

function complexIsFunctionStrictTest()
{
    function replace_formats() {
        var o = ["text", 0];
        if (typeof o == "string") {
        } else if (typeof o === "function") {
        } else if (typeof o == "object" && typeof o[0] == "string") {
            return "PASS";
        }
        return "FAIL";
    };

    return "%d".replace(/%d/, replace_formats);
}
shouldBe("complexIsFunctionStrictTest()", "'PASS'");

var successfullyParsed = true;
