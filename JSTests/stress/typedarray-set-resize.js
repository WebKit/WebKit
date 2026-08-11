function shouldBe(actual, expected) {
    if (actual !== expected) {
        throw new Error(`actual: ${actual}, expected: ${expected}`);
    }
}

var rab = new ArrayBuffer(5, { maxByteLength: 10 });
var typedArray = new Int8Array(rab);
var log = [];

var growNumber = 0;
var grow = {
    valueOf() {
        log.push("grow");
        rab.resize(rab.byteLength + 1);
        return --growNumber;
    },
};

var shrinkNumber = 0;
var shrink = {
    valueOf() {
        log.push("shrink");
        rab.resize(rab.byteLength - 1);
        return ++shrinkNumber;
    },
};

var array = {
    get length() {
        return 5;
    },
    0: shrink,
    1: shrink,
    2: shrink,
    3: grow,
    4: grow,
};

typedArray.set(array);

var expectedLogs = ["shrink", "shrink", "shrink", "grow", "grow"];
shouldBe(log.length, expectedLogs.length);
for (var i = 0; i < expectedLogs.length; ++i) {
    shouldBe(log[i], expectedLogs[i]);
}

var expectedTypedArray = [1, 2, 0, 0];
shouldBe(typedArray.length, expectedTypedArray.length);
for (var i = 0; i < expectedTypedArray.length; ++i) {
    shouldBe(typedArray[i], expectedTypedArray[i]);
}
