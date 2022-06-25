description(
"This test checks whether garbage collection can handle deeply nested objects.  It passes if the test does not crash."
);

var start = new Date;
var o = {};
var a = [];
for (var i = 0; i < 250000; i++) {
    o = {o:o};
    a=[a];
}

gc();
