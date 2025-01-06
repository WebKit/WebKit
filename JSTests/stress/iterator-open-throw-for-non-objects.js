function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const runs = 1e5;

let count = 0;
for (let i = 0; i < runs; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return 3; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, runs);
count = 0;

for (let i = 0; i < runs; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return "foo"; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, runs);
count = 0;

for (let i = 0; i < runs; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return true; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, runs);
count = 0;

for (let i = 0; i < runs; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return null; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, runs);
count = 0;

for (let i = 0; i < runs; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return undefined; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, runs);
count = 0;

for (let i = 0; i < runs; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return Symbol("foo"); };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, runs);
count = 0;

for (let i = 0; i < runs; i++) {
    try {
        const arr = [101, 101];
        arr[Symbol.iterator] = function () { return 3n; };
        [] = arr;
    } catch (e) {
        count++;
    }
}
shouldBe(count, runs);
count = 0;
