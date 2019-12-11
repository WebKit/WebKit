description(
"Tests that having a bad time has correct cross frame behavior, if an instance object is created in a different global object than the affected prototype, and the prototype is assigned using __proto__."
);

jsTestIsAsync = true;

var ouches = 0;

var array;

function foo(thePrototype) {
    array = {};
    array.__proto__ = thePrototype;
    array.length = 10;
    for (var i = 0; i < 10; i+=2)
        array[i] = 42;
}

function evil(thePrototype) {
    for (var i = 0; i < 10; i+=2)
        thePrototype.__defineSetter__(i + 1, function() { ouches++; });
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
    "var thePrototype = {};\n" +
    "window.parent.foo(thePrototype);\n" +
    "window.parent.evil(thePrototype);\n" +
    "window.parent.bar();\n" +
    "window.parent.done();\n" +
    "</script></body></html>");
frame.contentDocument.close();
