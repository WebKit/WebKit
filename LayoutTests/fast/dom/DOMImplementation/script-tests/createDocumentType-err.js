description("createDocument tests modeled after mozilla's testing");

function stringForExceptionCode(c)
{
    var exceptionName;
    switch(c) {
        case DOMException.INVALID_CHARACTER_ERR:
            exceptionName = "INVALID_CHARACTER_ERR";
            break;
        case DOMException.NAMESPACE_ERR:
            exceptionName = "NAMESPACE_ERR";
    }
    if (exceptionName)
        return exceptionName; // + "(" + c + ")";
    return c;
}

function assertEquals(actual, expect, m)
{
    if (actual !== expect) {
        m += "; expected " + stringForExceptionCode(expect) + ", threw " + stringForExceptionCode(actual);
        testFailed(m);
    } else {
        m += "; threw " + stringForExceptionCode(actual);;
        testPassed(m);
    }
}

var allTests = [
   { args: [undefined, undefined], name: "TypeError" },
   { args: [null, undefined], name: "TypeError"  },
   { args: [undefined, null], name: "TypeError" },
   { args: [undefined, undefined, null] },
   { args: [null, null], name: "TypeError" },
   { args: [null, null, null] },
   { args: [null, ""], name: "TypeError" },
   { args: ["", null], name: "TypeError" },
   { args: ["", ""], name: "TypeError" },
   { args: ["a:", null, null], code: 5 },
   { args: [":foo", null, null], code: 5 },
   { args: [":", null, null], code: 5 },
   { args: ["foo", null, null] },
   { args: ["foo:bar", null, null] },
   { args: ["foo::bar", null, null], code: 5 },
   { args: ["\t:bar", null, null], code: 5 },
   { args: ["foo:\t", null, null], code: 5 },
   { args: ["foo :bar", null, null], code: 5 },
   { args: ["foo: bar", null, null], code: 5 },
   { args: ["a:b:c", null, null], code: 5, message: "valid XML name, invalid QName" },
];

function sourceify(v)
{
    switch (typeof v) {
        case "undefined":
            return v;
        case "string":
            return '"' + v.replace('"', '\\"') + '"';
        default:
            return String(v);
    }
}

function sourceifyArgs(args)
{
    var copy = new Array(args.length);
    for (var i = 0, sz = args.length; i < sz; i++)
        copy[i] = sourceify(args[i]);

    return copy.join(", ");
}

function runTests(tests, createFunctionName)
{
    for (var i = 0, sz = tests.length; i < sz; i++) {
        var test = tests[i];

        var code = -1;
        var argStr = sourceifyArgs(test.args);
        var msg = createFunctionName + "(" + argStr + ")";
        if ("message" in test)
            msg += "; " + test.message;
        try {
            document.implementation[createFunctionName].apply(document.implementation, test.args);
            if ('code' in test)
                testFailed(msg + " expected exception: " + test.code);
            else if ('name' in test)
                testFailed(msg + " expected exception: " + test.name);
            else
                testPassed(msg);
        } catch (e) {
            exceptionThrown = e;
            if ('name' in test) {
                shouldBeEqualToString("exceptionThrown.name", "" + test.name);
            } else
                assertEquals(e.code, test.code || "expected no exception", msg);
        }
    }
}

shouldThrow("document.implementation.createDocumentType('foo')");
shouldThrow("document.implementation.createDocumentType('foo', null)");
runTests(allTests, "createDocumentType");
