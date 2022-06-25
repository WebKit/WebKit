description(
"This tests that function inlining in the DFG JIT doesn't get confused by different global objects."
);

jsTestIsAsync = true;

var b = 321;

function foo(a) {
    return a + b;
}

shouldBe("foo(3)", "324");

function done(value) {
    var expected = 5770500;
    if (value == expected)
        testPassed("done() called with " + expected);
    else
        testFailed("done() called with " + value + ", but expected " + expected);
    finishJSTest();
}

function doit() {
    document.getElementById("frameparent").innerHTML = "";
    document.getElementById("frameparent").innerHTML = "<iframe id='testframe'>";
    var testFrame = document.getElementById("testframe");
    testFrame.contentDocument.open();
    
    code = "<!DOCTYPE html>\n<head></head><body><script type=\"text/javascript\">\n";
    
    // Make sure that we get as many variables as the parent.
    for (var i = 0; i < 100; ++i)
        code += "var b" + i + " = " +i + ";\n";
    
    code += "result = 0;\n" +
        "function bar(a) {\n" +
        "    var foo = window.parent.foo;\n" +
        "    return ";
    
    for (var i = 0; i < 100; ++i)
        code += "b" + i + " + ";
    
    code += "foo(a);\n" +
        "}\n" +
        "for (var i = 0; i < 1000; ++i) {\n" +
        "    result += bar(i);\n" +
        "}\n" +
        "window.parent.done(result);\n" +
        "</script></body></html>"
    
    testFrame.contentDocument.write(code);
    testFrame.contentDocument.close();
}

window.setTimeout(doit, 10);

