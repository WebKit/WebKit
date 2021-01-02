function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function test1({ data }) {
    return data;
}

shouldBe(test1({ data: 42 }), 42);

shouldThrow(() => {
    const data = 42;
    ({ data } = { data: 50 });
}, `TypeError: Attempted to assign to readonly property.`);

(function () {
    const { data } = { data: 50 };
    shouldBe(data, 50);
}());

shouldThrow(() => {
    const { data } = data;
}, `ReferenceError: Cannot access uninitialized variable.`);

shouldThrow(() => {
    const { [throwing()]: data } = { data: 50 };
    function throwing() {
        data;
    }
}, `ReferenceError: Cannot access uninitialized variable.`);

(function () {
    let data = 42;
    ({ data } = { data: 50 });
    shouldBe(data, 50);
}());

(function () {
    let { data } = { data: 50 };
    shouldBe(data, 50);
}());

shouldThrow(() => {
    let { data } = data;
}, `ReferenceError: Cannot access uninitialized variable.`);

shouldThrow(() => {
    let { [throwing()]: data } = { data: 50 };
    function throwing() {
        data;
    }
}, `ReferenceError: Cannot access uninitialized variable.`);

shouldThrow(() => {
    let { [data = 'data']: data } = { data: 50 };
}, `ReferenceError: Cannot access uninitialized variable.`);

(function () {
    let { [43]: data } = { 43: 50 };
    shouldBe(data, 50);
}());

(function () {
    let called1 = false;
    let called2 = false;
    let data = 42;
    ({ [throwing()]: data, ...ok } = { data: 50 });
    function throwing() {
        called1 = true;
        shouldBe(data, 42);
        return {
            toString() {
                called2 = true;
                shouldBe(data, 42);
                return 'data';
            }
        };
    }
    shouldBe(data, 50);
    shouldBe(called1, true);
    shouldBe(called2, true);
}());

(function () {
    let data = 'ok';
    let obj;
    ({ [data]: data, ...obj } = { ok: 'bad', bad: 42 });
    shouldBe(data, 'bad');
    shouldBe(obj.bad, 42);
}());

(function () {
    globalThis.testing = 42;
    ({ testing } = { testing: 50 });
    shouldBe(testing, 50);
}());

(function () {
    var { data } = { data: 50 };
    shouldBe(data, 50);
}());

(function () {
    var { data } = { data: data };
    shouldBe(data, undefined);
}());

Reflect.defineProperty(globalThis, 'readOnly', {
    value: 30
});

shouldThrow(() => {
    'use strict';
    ({ readOnly } = { readOnly: 42 });
}, `TypeError: Attempted to assign to readonly property.`);

(function () {
    ({ readOnly } = { readOnly: 42 });
    shouldBe(readOnly, 30);
}());
