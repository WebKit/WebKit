description(
"This test checks whether the GC collects after string appends."
);

// FIXME: This test appears to be highly tied to the details of how JS strings report memory
// cost to the garbage collector.  It should be improved to be less tied to these implementation details.
// <https://bugs.webkit.org/show_bug.cgi?id=20871>

if (window.testRunner)
    testRunner.dumpAsText();

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
