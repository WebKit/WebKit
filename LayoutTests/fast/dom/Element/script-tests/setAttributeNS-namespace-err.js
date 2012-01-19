description("setAttributeNS tests adapted from createAttributeNS which in turn were adapted from createElementNS tests attached to webkit bug 16833");

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
   { args: ["http://www.w3.org/2000/xmlns/", "xmlns"] }, // See http://www.w3.org/2000/xmlns/
   { args: ["http://example.com/", "xmlns"], code: 14 }, // from the createAttributeNS section
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

function runNSTests()
{
    var element = document.createElement("div");
    var setFunction = element.setAttributeNS;
    var setFunctionName = "element.setAttributeNS";
    var value = "'value'";

    for (var i = 0, sz = allNSTests.length; i < sz; i++) {
        var test = allNSTests[i];

        var code = -1;
        var argStr = sourceify(test.args[0]) + ", " + sourceify(test.args[1]) + ", " + value;
        var msg = setFunctionName + "(" + argStr + ")";
        if ("message" in test)
            msg += "; " + test.message;
        try {
            setFunction.apply(element, test.args.concat([value]));
            assert(!("code" in test), msg);
        } catch (e) {
            assertEquals(e.code, test.code || "expected no exception", msg);
        }
    }
}

// Moz throws a "Not enough arguments" exception in these, we don't:
var element = document.createElement("div");
shouldBeUndefined("element.setAttributeNS()");
shouldBeUndefined("element.setAttributeNS(\"http://www.example.com\")");
shouldBeUndefined("element.setAttributeNS(\"http://www.example.com\", \"foo\")");

runNSTests();
