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
   { args: [undefined, undefined], code: 5  },
   { args: [null, undefined], code: 5  },
   { args: [undefined, null], code: 5 },
   { args: [undefined, undefined, null], code: 5 },
   { args: [null, null], code: 5 },
   { args: [null, null, null], code: 5 },
   { args: [null, ""], code: 5 },
   { args: ["", null], code: 5 },
   { args: ["", ""], code: 5 },
   { args: ["a:", null, null], code: 14 },
   { args: [":foo", null, null], code: 14 },
   { args: [":", null, null], code: 14 },
   { args: ["foo", null, null] },
   { args: ["foo:bar", null, null] },
   { args: ["foo::bar", null, null], code: 14 },
   { args: ["\t:bar", null, null], code: 5 },
   { args: ["foo:\t", null, null], code: 5 },
   { args: ["foo :bar", null, null], code: 5 },
   { args: ["foo: bar", null, null], code: 5 },
   { args: ["a:b:c", null, null], code: 14, message: "valid XML name, invalid QName" },
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
            else
                testPassed(msg);
        } catch (e) {
            assertEquals(e.code, test.code || "expected no exception", msg);
        }
    }
}

// Moz throws a "Not enough arguments" exception in these, we don't:
shouldBeEqualToString("document.implementation.createDocumentType('foo').toString()", "[object DocumentType]");
shouldBeEqualToString("document.implementation.createDocumentType('foo', null).toString()", "[object DocumentType]");
runTests(allTests, "createDocumentType");

var successfullyParsed = true;
