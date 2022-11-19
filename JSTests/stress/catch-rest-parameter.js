function assert(b) {
    if (!b)
        throw "bad assert!";
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
        throw new Error(`
            bad error:      ${String(error)}
            expected error: ${errorMessage}
        `);
}

let expected = "TypeError: undefined is not an object (evaluating '[[]]')";

// AsyncFunction
(() => {
    async function f(...[[]]) { }
    let isExpected = false;
    f().then(e => isExpected = false).catch(e => isExpected = e == expected);
    drainMicrotasks();
    assert(isExpected);
})();

(() => {
    async function f([[]]) { }
    let isExpected = false;
    f().then(e => isExpected = false).catch(e => isExpected = e == expected);
    drainMicrotasks();
    assert(isExpected);
})();

(() => {
    async function f(a, ...[[]]) { }
    let isExpected = false;
    f().then(e => isExpected = false).catch(e => isExpected = e == expected);
    drainMicrotasks();
    assert(isExpected);
})();

(() => {
    var a = 1, b = 2, c = 3;
    async function f(arg0, ...args) {
        a = arg0;
        b = args[0];
        c = args[1];
    }
    let isExpected = true;
    f(4, 5, 6).then(e => isExpected = true).catch(e => isExpected = false);
    drainMicrotasks();
    assert(isExpected && a === 4 && b === 5 && c === 6);
})();


// AsyncArrowFunction
(() => {
    let f = async (...[[]]) => { };
    let isExpected = false;
    f().then(e => isExpected = false).catch(e => isExpected = e == expected);
    drainMicrotasks();
    assert(isExpected);
})();

(() => {
    let f = async ([[]]) => { };
    let isExpected = false;
    f().then(e => isExpected = false).catch(e => isExpected = e == expected);
    drainMicrotasks();
    assert(isExpected);
})();

(() => {
    let f = async (a, ...[[]]) => { };
    let isExpected = false;
    f().then(e => isExpected = false).catch(e => isExpected = e == expected);
    drainMicrotasks();
    assert(isExpected);
})();

(() => {
    var a = 1, b = 2, c = 3;
    let f = async (arg0, ...args) => {
        a = arg0;
        b = args[0];
        c = args[1];
    };
    let isExpected = true;
    f(4, 5, 6).then(e => isExpected = true).catch(e => isExpected = false);
    drainMicrotasks();
    assert(isExpected && a === 4 && b === 5 && c === 6);
})();


// AsyncMethod
(() => {
    class x { static async f(...[[]]) { } }
    let isExpected = false;
    x.f().then(e => isExpected = false).catch(e => isExpected = e == expected);
    drainMicrotasks();
    assert(isExpected);
})();

(() => {
    class x { static async f([[]]) { } }
    let isExpected = false;
    x.f().then(e => isExpected = false).catch(e => isExpected = e == expected);
    drainMicrotasks();
    assert(isExpected);
})();

(() => {
    class x { static async f(a, ...[[]]) { } }
    let isExpected = false;
    x.f().then(e => isExpected = false).catch(e => isExpected = e == expected);
    drainMicrotasks();
    assert(isExpected);
})();

(() => {
    var a = 1, b = 2, c = 3;
    class x {
        static async f(arg0, ...args) {
            a = arg0;
            b = args[0];
            c = args[1];
        }
    }
    let isExpected = true;
    x.f(4, 5, 6).then(e => isExpected = true).catch(e => isExpected = false);
    drainMicrotasks();
    assert(isExpected && a === 4 && b === 5 && c === 6);
})();

// AsyncGenerator
shouldThrow(async function* f([[]]) { }, expected);
shouldThrow(async function* f(...[[]]) { }, expected);
shouldThrow(async function* f(a, ...[[]]) { }, expected);
(() => {
    var a = 1, b = 2, c = 3;
    async function* f(arg0, ...args) {
        a = arg0;
        b = args[0];
        c = args[1];
    }
    f(4, 5, 6);
    drainMicrotasks();
    assert(a === 1 && b === 2 && c === 3);
})();

// Generator
shouldThrow(function* f([[]]) { }, expected);
shouldThrow(function* f(...[[]]) { }, expected);
shouldThrow(function* f(a, ...[[]]) { }, expected);
(() => {
    var a = 1, b = 2, c = 3;
    function* f(arg0, ...args) {
        a = arg0;
        b = args[0];
        c = args[1];
    }
    f(4, 5, 6);
    assert(a === 1 && b === 2 && c === 3);
})();

// Function
shouldThrow(function f([[]]) { }, expected);
shouldThrow(function f(...[[]]) { }, expected);
shouldThrow(function f(a, ...[[]]) { }, expected);
(() => {
    var a = 1, b = 2, c = 3;
    function f(arg0, ...args) {
        a = arg0;
        b = args[0];
        c = args[1];
    }
    f(4, 5, 6);
    assert(a === 4 && b === 5 && c === 6);
})();
