description(
"This tests that passing an inconsistent compareFn to sort() doesn't cause a crash."
);

for (var attempt = 0; attempt < 100; ++attempt) {
    var arr = [];
    for (var i = 0; i < 64; ++i)
        arr[i] = i;
    arr.sort(function() { return 0.5 - Math.random(); });
}

// Sorting objects that change each time sort() looks at them is the same as using a random compareFn.
function RandomObject() {
    this.toString = function() { return (Math.random() * 100).toString(); }
}

for (var attempt = 0; attempt < 100; ++attempt) {
    var arr = [];
    for (var i = 0; i < 64; ++i)
        arr[i] = new RandomObject;
    arr.sort();
}

var successfullyParsed = true;
