// Regression test for https://bugs.webkit.org/show_bug.cgi?id=157322.  This test should not crash.

var fromArray = [];
var toArray = [];
var dummyArray = [];
var endObj1 = {
    valueOf: function() {
        var originalLength = fromArray.length;
        fromArray.length = 1;

        dummyArray = new Float64Array(1000);

        return originalLength;
    }
};

var endObj2 = {
    valueOf: function() {
        var originalLength = fromArray.length;
        fromArray.length = 1;

        dummyArray = new Float64Array(1000);

        fromArray = [];
        fromArray.length = originalLength;

        return originalLength;
    }
};

var initialArray = [];
for (var i = 0; i < 8000; i++)
        initialArray.push(i + 0.1);

for (var loop = 0; loop < 1000; loop++) {
    fromArray = initialArray.slice(0);

    var endObj = (loop % 2 == 1) ? endObj1 : endObj2;

    // These calls shouldn't crash
    toArray = fromArray.slice(0, endObj);
    toArray = fromArray.splice(0, endObj);
}
