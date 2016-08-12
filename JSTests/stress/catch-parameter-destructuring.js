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

class CallGuard {
    constructor()
    {
        this.called = false;
    }

    call()
    {
        this.called = true;
    }
}

(function () {
    let guard = new CallGuard();
    try {
        throw { value: 42, done: false };
    } catch ({ value, done }) {
        shouldBe(value, 42);
        shouldBe(done, false);
        guard.call();
    }
    shouldBe(guard.called, true);
}());

(function () {
    let guard = new CallGuard();
    try {
        throw { value: 42, done: false };
    } catch ({ value: v, done: d }) {
        shouldBe(v, 42);
        shouldBe(d, false);
        guard.call();
    }
    shouldBe(guard.called, true);
    // lexical
    shouldBe(typeof v, "undefined");
    shouldBe(typeof d, "undefined");
}());

shouldThrow(function () {
    try {
        throw { get error() { throw new Error("OK"); } };
    } catch ({ error }) {
    }
}, `Error: OK`);

let guard = new CallGuard();
shouldThrow(function () {
    try {
        throw { get error() { throw new Error("OK"); } };
    } catch ({ error }) {
    } finally {
        guard.call();
    }
}, `Error: OK`);
shouldBe(guard.called, true);

(function initialize() {
    let guard = new CallGuard();
    try {
        throw { value: 42, done: false };
    } catch ({ value, done, hello = 44 }) {
        shouldBe(value, 42);
        shouldBe(done, false);
        shouldBe(hello, 44);
        guard.call();
    }
    shouldBe(guard.called, true);
}());

(function array() {
    let guard = new CallGuard();
    try {
        throw [0, 1, 2, 3, 4, 5];
    } catch ([ a, b, c, ...d ]) {
        shouldBe(a, 0);
        shouldBe(b, 1);
        shouldBe(c, 2);
        shouldBe(JSON.stringify(d), `[3,4,5]`);
        guard.call();
    }
    shouldBe(guard.called, true);
}());

(function generator() {
    function *gen(v) {
        try {
            throw v;
        } catch ({ value = yield 42 }) {
            yield value;
        }
    }

    {
        let g = gen({});
        {
            let { value, done } = g.next();
            shouldBe(value, 42);
            shouldBe(done, false);
        }
        {
            let { value, done } = g.next("OK");
            shouldBe(value, "OK");
            shouldBe(done, false);
        }
        {
            let { value, done } = g.next("OK");
            shouldBe(value, undefined);
            shouldBe(done, true);
        }
    }

    {
        let g = gen({value: 400});
        {
            let { value, done } = g.next();
            shouldBe(value, 400);
            shouldBe(done, false);
        }
        {
            let { value, done } = g.next("OK");
            shouldBe(value, undefined);
            shouldBe(done, true);
        }
    }
}());
