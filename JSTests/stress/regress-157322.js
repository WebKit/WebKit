// Regression test for https://bugs.webkit.org/show_bug.cgi?id=157322.  This test should not crash.

let fromArray = [];
let toArray = [];
let dummyArray = [];
let endObj1 = {
    valueOf: function() {
        let originalLength = fromArray.length;
        fromArray.length = 1;

        dummyArray = new Float64Array(1000);

        return originalLength;
    }
};

let endObj2 = {
    valueOf: function() {
        let originalLength = fromArray.length;
        fromArray.length = 1;

        dummyArray = new Float64Array(1000);

        fromArray = [];
        fromArray.length = originalLength;

        return originalLength;
    }
};

let initialArray = [];
for (let i = 0; i < 8000; i++)
        initialArray.push(i + 0.1);

for (let loop = 0; loop < 1000; loop++) {
    fromArray = initialArray.slice(0);

    let endObj = (loop % 2 == 1) ? endObj1 : endObj2;

    // These calls shouldn't crash
    toArray = fromArray.slice(0, endObj);
    toArray = fromArray.splice(0, endObj);
}
