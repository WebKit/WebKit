description(
"This test checks whether the GC collects after string appends."
);

if (window.layoutTestController)
    layoutTestController.dumpAsText();

if (window.GCController)
    GCController.collect();


// str has 150 chars in it (which is greater than the limit of the GC to ignore which I believe is at 128).
var str = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
var count = 500;
for (var i = 0; i < count; ++i) {
    str += "b";
}

// str has 128 chars in it.
str = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
count = 10;
for (var i = 0; i < count; ++i) {
    str += str;
}

var jsObjCount = 0;
if (window.GCController)
    jsObjCount = GCController.getJSObjectCount();

if (jsObjCount <= 500 && jsObjCount > 0)
    testPassed("Garbage Collector triggered")
else
    testFailed("Garbage Collector NOT triggered. Number of JSObjects: " + jsObjCount);

var successfullyParsed = true;
