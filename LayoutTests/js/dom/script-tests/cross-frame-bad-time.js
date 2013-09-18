description(
"Tests that having a bad time has correct cross frame behavior."
);

if (window.testRunner)
    testRunner.waitUntilDone();

var ouches = 0;

function foo(array) {
    for (var i = 0; i < 100; ++i)
        array[0] = true;
    if (ouches == 100)
        testPassed("Got 100 ouches.");
    else
        testFailed("Did not get 100 ouches. Got " + ouches + " instead.");
    if (testRunner)
        testRunner.notifyDone();
}

var frame = document.getElementById("myframe");

frame.contentDocument.open();
frame.contentDocument.write(
    "<!DOCTYPE html>\n<html><body><script type=\"text/javascript\">\n" +
    "Array.prototype.__defineSetter__(0, function() { window.parent.ouches++; });\n" +
    "window.parent.foo([]);\n" +
    "</script></body></html>");
frame.contentDocument.close();
