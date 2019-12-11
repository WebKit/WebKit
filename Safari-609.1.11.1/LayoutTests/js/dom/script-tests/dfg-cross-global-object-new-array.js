description(
"This tests that function inlining in the DFG JIT doesn't get confused about the global object to use for array allocation."
);

window.jsTestIsAsync = true;

function foo(o) {
    return new o.arrayConstructor();
}

function runTest(arrayConstructor) {
    var o = {arrayConstructor: arrayConstructor};
    
    noInline(foo);
    while (!dfgCompiled({f:foo}))
        foo(o);
    
    var array = foo(o);
    
    if (array.__proto__ == Array.prototype)
        testFailed("Array has the main global object's array prototype");
    else
        testPassed("Array doesn't have the main global object's array prototype");
    finishJSTest();
}

function doit() {
    document.getElementById("frameparent").innerHTML = "";
    document.getElementById("frameparent").innerHTML = "<iframe id='testframe'>";
    var testFrame = document.getElementById("testframe");
    testFrame.contentDocument.open();
    
    code  = "<!DOCTYPE html>\n<head></head><body><script type=\"text/javascript\">\n";
    code += "window.parent.runTest(Array);\n";
    code += "</script></body></html>";
    
    testFrame.contentDocument.write(code);
    testFrame.contentDocument.close();
}

window.setTimeout(doit, 0);

