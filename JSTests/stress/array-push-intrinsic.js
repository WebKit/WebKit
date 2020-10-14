"use strict";

const arrayPush = $vm.createBuiltin("(function (a, v) { @arrayPush(a, v); })");
noInline(arrayPush);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}.`);
}

function shouldThrow(func, errorMessage)
{
    var errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

shouldThrow(
    () => arrayPush(x, y),
    "ReferenceError: Can't find variable: x",
);

shouldThrow(
    () => arrayPush({ get length() { throw new Error("'length' should be unreachable."); } }, y),
    "ReferenceError: Can't find variable: y",
);

for (let i = 0; i < 5; ++i) {
    Object.defineProperty(Object.prototype, i, {
        get() { throw new Error(i + " getter should be unreachable."); },
        set(_value) { throw new Error(i + " setter should be unreachable."); },
    });
}

(() => {
    const arr = [];
    for (let i = 0; i < 1e5; ++i) {
        arrayPush(arr, i);
        shouldBe(arr[i], i);
        shouldBe(arr.length, i + 1);
    }
})();

(() => {
    const maxLength = 2 ** 32 - 1;
    const startIndex = maxLength - 1e4;
    const arr = new Array(startIndex);

    for (let i = 0; i < 1e4; ++i) {
        arrayPush(arr, i);
        shouldBe(arr[startIndex + i], i);
        shouldBe(arr.length, startIndex + i + 1);
    }

    shouldBe(arr.length, maxLength);

    for (let i = 0; i < 5; ++i) {
        Object.defineProperty(Object.prototype, maxLength + i, {
            get() { throw new Error(i + " getter should be unreachable."); },
            set(_value) { throw new Error(i + " setter should be unreachable."); },
        });
    }

    for (let i = 1; i < 1e4; ++i) {
        arrayPush(arr, i);
        shouldBe(arr.hasOwnProperty(maxLength + i), false);
        shouldBe(arr.length, maxLength);
    }
})();
