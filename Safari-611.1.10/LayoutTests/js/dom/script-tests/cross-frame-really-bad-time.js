description(
"Tests that having a bad time has correct cross frame behavior, if an instance object is created in a different global object than the affected prototype."
);

jsTestIsAsync = true;

var ouches = 0;

function Cons() { }

var array;

function foo() {
    array = new Cons();
    array.length = 10;
    for (var i = 0; i < 10; i+=2)
        array[i] = 42;
}

function evil() {
    for (var i = 0; i < 10; i+=2)
        Cons.prototype.__defineSetter__(i + 1, function() { ouches++; });
}

function bar() {
    for (var i = 0; i < 10; i+=2)
        array[i + 1] = 63;
}

function done() {
    var string = Array.prototype.join.apply(array, [","]);
    debug("Array is: " + string);
    if (string == "42,,42,,42,,42,,42,")
        testPassed("Array has holes in odd numbered entries.");
    else
        testFailed("Array does not have the required holes.");
    
    if (ouches == 5)
        testPassed("Got 5 ouches.");
    else
        testFailed("Did not get 5 ouches. Got " + ouches + " + instead.");
    
    finishJSTest();
}

var frame = document.getElementById("myframe");

frame.contentDocument.open();
frame.contentDocument.write(
    "<!DOCTYPE html>\n<html><body><script type=\"text/javascript\">\n" +
    "window.parent.Cons.prototype = {};\n" +
    "window.parent.foo();\n" +
    "window.parent.evil();\n" +
    "window.parent.bar();\n" +
    "window.parent.done();\n" +
    "</script></body></html>");
frame.contentDocument.close();
