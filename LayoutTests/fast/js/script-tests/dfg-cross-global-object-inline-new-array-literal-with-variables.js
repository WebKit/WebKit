description(
"This tests that function inlining in the DFG JIT doesn't get confused about the global object to use for array allocation."
);

window.jsTestIsAsync = true;

function foo(x) {
    return [x, x + 1, x * 2];
}

Array.prototype.thingy = 24;

function done(value) {
    var expected = (24 + 3) * 200 + 19900 + 20100 + 39800;
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
    
    code  = "<!DOCTYPE html>\n<head></head><body><script type=\"text/javascript\">\n";
    code += "Array.prototype.thingy = 42;\n";
    code += "function bar(x) {\n";
    code += "    return window.parent.foo(x);\n";
    code += "}\n";
    code += "var result = 0;\n";
    code += "for (var i = 0; i < 200; ++i) {\n";
    code += "    var theArray = bar(i);\n";
    code += "    result += theArray.thingy + theArray.length + theArray[0] + theArray[1] + theArray[2];\n";
    code += "}\n";
    code += "window.parent.done(result);\n";
    code += "</script></body></html>";
    
    testFrame.contentDocument.write(code);
    testFrame.contentDocument.close();
}

window.setTimeout(doit, 10);

