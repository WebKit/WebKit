function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

let count = 0;
for (let i = 0; i < testLoopCount; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return 3; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, testLoopCount);
count = 0;

for (let i = 0; i < testLoopCount; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return "foo"; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, testLoopCount);
count = 0;

for (let i = 0; i < testLoopCount; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return true; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, testLoopCount);
count = 0;

for (let i = 0; i < testLoopCount; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return null; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, testLoopCount);
count = 0;

for (let i = 0; i < testLoopCount; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return undefined; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, testLoopCount);
count = 0;

for (let i = 0; i < testLoopCount; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return Symbol("foo"); };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, testLoopCount);
count = 0;

for (let i = 0; i < testLoopCount; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return 3n; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, testLoopCount);
count = 0;
