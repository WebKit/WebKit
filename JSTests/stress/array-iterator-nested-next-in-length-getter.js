function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

var finishedReads = 0;
var finishedIterator;
var finishedArray = {
    0: "a",
    1: "b",
    get length() {
        finishedReads++;
        if (finishedReads === 1) {
            finishedIterator.next();
            return 2;
        }
        return finishedReads === 2 ? 0 : 2;
    }
};
finishedIterator = Array.prototype.values.call(finishedArray);
var first = finishedIterator.next();
shouldBe(first.value, "a");
shouldBe(first.done, false);
var finished = finishedIterator.next();
shouldBe(finished.value, undefined);
shouldBe(finished.done, true);
shouldBe(finishedReads, 2);

var rewoundReads = 0;
var rewoundIterator;
var rewoundArray = {
    0: "a",
    1: "b",
    2: "c",
    get length() {
        rewoundReads++;
        if (rewoundReads === 1) {
            rewoundIterator.next();
            rewoundIterator.next();
        }
        return 3;
    }
};
rewoundIterator = Array.prototype.values.call(rewoundArray);
var rewoundFirst = rewoundIterator.next();
shouldBe(rewoundFirst.value, "a");
shouldBe(rewoundFirst.done, false);
var rewoundSecond = rewoundIterator.next();
shouldBe(rewoundSecond.value, "b");
shouldBe(rewoundSecond.done, false);
