description(
"This tests that function inlining in the DFG JIT doesn't get confused about the global object to use for array allocation."
);

window.jsTestIsAsync = true;

function foo() {
    return [1, 2, 3];
}

Array.prototype.thingy = 24;

function done(value) {
    var expected = 1 + 2 + 3 + 3 + 24;
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
    code += "function bar() {\n";
    code += "    return window.parent.foo();\n";
    code += "}\n";
    code += "window.parent.noInline(bar);\n";
    code += "var theArray;\n";
    code += "while (!window.parent.dfgCompiled({f:bar}))\n";
    code += "    theArray = bar();\n";
    code += "var result = theArray.thingy + theArray.length + theArray[0] + theArray[1] + theArray[2];\n";
    code += "window.parent.done(result);\n";
    code += "</script></body></html>";
    
    testFrame.contentDocument.write(code);
    testFrame.contentDocument.close();
}

window.setTimeout(doit, 0);

