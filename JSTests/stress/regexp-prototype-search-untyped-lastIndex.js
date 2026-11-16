function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}
function shouldThrow(func, expected) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('not thrown');
    shouldBe(String(error), expected);
}
var errorKey = {
    toString() {
        throw new Error('out');
    }
};

let catchCount = 0;
let expectedCatchCount = testLoopCount / 2;

var string = 'Cocoa, Cappuccino, Rize, Matcha, Kilimanjaro';
const cocoaRe = /Cocoa/g;
for (let i = 0; i < testLoopCount; i ++) {
    if (i === expectedCatchCount)
        Object.defineProperty(cocoaRe, "lastIndex", { writable: false });
    shouldThrow(function () { string.search(errorKey); }, "Error: out");
    shouldBe(string.search(/Rize/), 19);
    shouldBe(string.search('Rize'), 19);
    shouldBe(string.search(/Matcha/), 25);
    try {
        shouldBe('Cocoa'.search(cocoaRe), 0);
    } catch {
        catchCount++;
    }
}

shouldBe(catchCount, expectedCatchCount);
