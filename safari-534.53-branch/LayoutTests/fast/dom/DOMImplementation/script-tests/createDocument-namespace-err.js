description("createDocument tests modeled after createElementNS tests from mozilla which were attached to webkit bug 16833");

// document.implementation.createDocument() should throw the same set of errors
// as document.createElementNS()
// http://www.w3.org/TR/DOM-Level-3-Core/core.html#Level-2-Core-DOM-createDocument
// Thus we copied these test cases from:
// LayoutTests/fast/dom/Document/resources/createDocument-namespace-err.js

function assert(c, m)
{
    if (!c)
        testFailed(m);
    else
        testPassed(m);
}

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

var allNSTests = [
   { args: [undefined, undefined] },
   { args: [null, undefined] },
   { args: [undefined, null], code: 5 },
   { args: [null, null], code: 5 },
   { args: [null, ""], code: 5 },
   { args: ["", null], code: 5 },
   { args: ["", ""], code: 5 },
   { args: [null, "<div>"], code: 5 },
   { args: [null, "0div"], code: 5 },
   { args: [null, "di v"], code: 5 },
   { args: [null, "di<v"], code: 5 },
   { args: [null, "-div"], code: 5 },
   { args: [null, ".div"], code: 5 },
   { args: ["http://example.com/", "<div>"], code: 5 },
   { args: ["http://example.com/", "0div"], code: 5 },
   { args: ["http://example.com/", "di<v"], code: 5 },
   { args: ["http://example.com/", "-div"], code: 5 },
   { args: ["http://example.com/", ".div"], code: 5 },
   { args: [null, ":div"], code: 14 },
   { args: [null, "div:"], code: 14 },
   { args: ["http://example.com/", ":div"], code: 14 },
   { args: ["http://example.com/", "div:"], code: 14 },
   { args: [null, "d:iv"], code: 14 },
   { args: [null, "a:b:c"], code: 14, message: "valid XML name, invalid QName" },
   { args: ["http://example.com/", "a:b:c"], code: 14, message: "valid XML name, invalid QName" },
   { args: [null, "a::c"], code: 14, message: "valid XML name, invalid QName" },
   { args: ["http://example.com/", "a::c"], code: 14, message: "valid XML name, invalid QName" },
   { args: ["http://example.com/", "a:0"], code: 5, message: "valid XML name, not a valid QName" },
   { args: ["http://example.com/", "0:a"], code: 5, message: "0 at start makes it not a valid XML name" },
   { args: ["http://example.com/", "a:_"] },
   { args: ["http://example.com/", "a:\u0BC6"], code: 14,
     message: "non-ASCII character after colon is CombiningChar, which is " +
              "NCNameChar but not (Letter | \"_\") so invalid at start of " +
              "NCName (but still a valid XML name, hence not INVALID_CHARACTER_ERR)" },
   { args: ["http://example.com/", "\u0BC6:a"], code: 5,
     message: "non-ASCII character after colon is CombiningChar, which is " +
              "NCNameChar but not (Letter | \"_\") so invalid at start of " +
              "NCName (Gecko chooses to throw NAMESPACE_ERR here, but either is valid " +
              "as this is both an invalid XML name and an invalid QName)" },
   { args: ["http://example.com/", "a:a\u0BC6"] },
   { args: ["http://example.com/", "a\u0BC6:a"] },
   { args: ["http://example.com/", "xml:test"], code: 14, message: "binding xml prefix wrong" },
   { args: ["http://example.com/", "xmlns:test"], code: 14, message: "binding xmlns prefix wrong" },
   { args: ["http://www.w3.org/2000/xmlns/", "x:test"], code: 14, message: "binding namespace namespace to wrong prefix" },
   { args: ["http://www.w3.org/2000/xmlns/", "xmlns:test"] },
   { args: ["http://www.w3.org/XML/1998/namespace", "xml:test"] },
   { args: ["http://www.w3.org/XML/1998/namespace", "x:test"] },
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

function runNSTests(tests, doc, createFunctionName)
{
    for (var i = 0, sz = tests.length; i < sz; i++) {
        var test = tests[i];

        // Gecko throws "undefined" if createDocument isn't
        // called with 3 arguments.  Instead of modifying all
        // of the values in the arrays above (which were taken from createElementNS tests)
        // we will instead just hack the args list here.
        var argsWithExtraLastNull = test.args.slice(); // copy the args arary
        argsWithExtraLastNull.push(null);

        var code = -1;
        var argStr = sourceifyArgs(argsWithExtraLastNull);
        var msg = createFunctionName + "(" + argStr + ")";
        if ("message" in test)
            msg += "; " + test.message;
        try {
            doc[createFunctionName].apply(doc, argsWithExtraLastNull);
            assert(!("code" in test), msg);
        } catch (e) {
            assertEquals(e.code, test.code || "expected no exception", msg);
        }
    }
}

// Moz throws a "Not enough arguments" exception in these, we don't:
shouldBeEqualToString("document.implementation.createDocument().toString()", "[object Document]");
shouldBeEqualToString("document.implementation.createDocument(\"http://www.example.com\").toString()", "[object Document]");

runNSTests(allNSTests, document.implementation, "createDocument");

var successfullyParsed = true;
