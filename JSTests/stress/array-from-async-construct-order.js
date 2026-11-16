function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

async function orderOnce() {
    var log = [];
    function C() { log.push("construct"); }
    var asyncItems = {
        [Symbol.asyncIterator]() {
            log.push("call Symbol.asyncIterator");
            return { get next() { log.push("get next"); return async function () { return { done: true }; }; } };
        },
    };
    var syncItems = {
        [Symbol.iterator]() {
            log.push("call Symbol.iterator");
            return { get next() { log.push("get next"); return function () { return { done: true }; }; } };
        },
    };
    await Array.fromAsync.call(C, asyncItems);
    shouldBe(log.join(", "), "call Symbol.asyncIterator, get next, construct");
    log = [];
    await Array.fromAsync.call(C, syncItems);
    shouldBe(log.join(", "), "call Symbol.iterator, get next, construct");
}
noInline(orderOnce);

async function collectValues() {
    var constructed = 0;
    function C() {
        constructed++;
        shouldBe(arguments.length, 0);
    }
    var iter = {
        i: 0,
        next() {
            if (this !== iter)
                throw new Error("next this");
            if (this.i < 3)
                return Promise.resolve({ value: this.i++, done: false });
            return Promise.resolve({ done: true });
        },
        [Symbol.asyncIterator]() { return this; },
    };
    var result = await Array.fromAsync.call(C, iter);
    shouldBe(constructed, 1);
    shouldBe(result instanceof C, true);
    shouldBeArray(result, [0, 1, 2]);
    shouldBe(result.length, 3);
}

async function nextReadOnce() {
    var gets = 0;
    var calls = 0;
    var iter = {
        i: 0,
        get next() {
            gets++;
            var self = this;
            return function () {
                calls++;
                if (this !== self)
                    throw new Error("next this");
                if (arguments.length !== 0)
                    throw new Error("next arguments");
                if (self.i < 3)
                    return { value: self.i++, done: false };
                return { done: true };
            };
        },
        [Symbol.asyncIterator]() { return this; },
    };
    var result = await Array.fromAsync(iter);
    shouldBe(gets, 1);
    shouldBe(calls, 4);
    shouldBeArray(result, [0, 1, 2]);
}

async function throwingNextSkipsConstruct() {
    var log = [];
    function C() { log.push("construct"); }
    var items = {
        [Symbol.asyncIterator]() {
            log.push("call");
            return { get next() { log.push("get next"); throw new Error("next"); } };
        },
    };
    var promise = Array.fromAsync.call(C, items);
    shouldBe(typeof promise.then, "function");
    var error;
    try {
        await promise;
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), "Error: next");
    shouldBe(log.join(", "), "call, get next");
}

async function constructThrowSkipsClose() {
    var log = [];
    function C() {
        log.push("construct");
        throw new Error("construct");
    }
    var items = {
        [Symbol.asyncIterator]() {
            log.push("call");
            return {
                get next() {
                    log.push("get next");
                    return function () { log.push("call next"); return { done: true }; };
                },
                get return() {
                    log.push("get return");
                    return function () { log.push("call return"); return { done: true }; };
                },
            };
        },
    };
    var error;
    try {
        await Array.fromAsync.call(C, items);
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), "Error: construct");
    shouldBe(log.join(", "), "call, get next, construct");
}

async function nextThrowSkipsReturn() {
    var log = [];
    function C() { log.push("construct"); }
    var items = {
        [Symbol.asyncIterator]() {
            log.push("call");
            return {
                get next() {
                    log.push("get next");
                    return function () {
                        log.push("call next");
                        throw new Error("step");
                    };
                },
                get return() {
                    log.push("get return");
                    return function () { log.push("call return"); return { done: true }; };
                },
            };
        },
    };
    var error;
    try {
        await Array.fromAsync.call(C, items);
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), "Error: step");
    shouldBe(log.join(", "), "call, get next, construct, call next");
}

async function mapThrowCloses() {
    var log = [];
    function C() { log.push("construct"); }
    var iter = {
        [Symbol.asyncIterator]() {
            log.push("call");
            return this;
        },
        get next() {
            log.push("get next");
            return function () {
                log.push("call next");
                if (this !== iter)
                    throw new Error("next this");
                return { value: 1, done: false };
            };
        },
        get return() {
            log.push("get return");
            return function () {
                log.push("call return");
                if (this !== iter)
                    throw new Error("return this");
                if (arguments.length !== 0)
                    throw new Error("return arguments");
                return { done: true };
            };
        },
    };
    var error;
    try {
        await Array.fromAsync.call(C, iter, function () { log.push("map"); throw new Error("map"); });
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), "Error: map");
    shouldBe(log.join(", "), "call, get next, construct, call next, map, get return, call return");
}

async function successSkipsReturn() {
    var log = [];
    var iter = {
        get next() {
            log.push("get next");
            return function () { return { done: true }; };
        },
        get return() {
            log.push("get return");
            return function () { log.push("call return"); return { done: true }; };
        },
        [Symbol.asyncIterator]() { log.push("call"); return this; },
    };
    var result = await Array.fromAsync(iter);
    shouldBe(result.length, 0);
    shouldBe(log.join(", "), "call, get next");
}

async function nonObjectIterator() {
    var previous = Object.getOwnPropertyDescriptor(Number.prototype, "next");
    Object.defineProperty(Number.prototype, "next", {
        configurable: true,
        get() { throw new Error("boxed next"); },
    });
    try {
        var log = [];
        function C() { log.push("construct"); }
        var items = {
            [Symbol.asyncIterator]() {
                log.push("call");
                return 1;
            },
        };
        var promise = Array.fromAsync.call(C, items);
        shouldBe(typeof promise.then, "function");
        var error;
        try {
            await promise;
        } catch (e) {
            error = e;
        }
        shouldBe(String(error), "TypeError: Iterator result interface is not an object.");
        shouldBe(log.join(", "), "call");
    } finally {
        if (previous)
            Object.defineProperty(Number.prototype, "next", previous);
        else
            delete Number.prototype.next;
    }
}

async function nullPrototypeIterator() {
    var iter = Object.create(null);
    var i = 0;
    iter.next = function () {
        if (this !== iter)
            throw new Error("next this");
        if (arguments.length !== 0)
            throw new Error("next arguments");
        if (i++ === 0)
            return { value: 7, done: false };
        return { done: true };
    };
    var result = await Array.fromAsync({ [Symbol.asyncIterator]() { return iter; } });
    shouldBeArray(result, [7]);
}

async function generatorClose() {
    var returned = false;
    async function* generator() {
        try {
            yield 1;
            yield 2;
        } finally {
            returned = true;
        }
    }
    var error;
    try {
        await Array.fromAsync(generator(), function () { throw new Error("map"); });
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), "Error: map");
    shouldBe(returned, true);

    returned = false;
    var result = await Array.fromAsync(generator());
    shouldBeArray(result, [1, 2]);
    shouldBe(returned, true);
}

async function arrayLikeStillConstructsWithLength() {
    var args = null;
    function C(length) { args = length; }
    var result = await Array.fromAsync.call(C, { length: 2, 0: Promise.resolve(4), 1: 5 });
    shouldBe(args, 2);
    shouldBe(result instanceof C, true);
    shouldBeArray(result, [4, 5]);
}

async function closeDoesNotAddAnAwait() {
    var log = [];
    var iter = {
        next() { return { value: 1, done: false }; },
        get return() {
            log.push("get return");
            return undefined;
        },
        [Symbol.asyncIterator]() { return this; },
    };
    var promise = Array.fromAsync(iter, function () {
        log.push("map");
        throw new Error("map");
    });
    function tick(n) {
        log.push(n);
        if (n < 3)
            Promise.resolve().then(function () { tick(n + 1); });
    }
    Promise.resolve().then(function () { tick(0); });
    var atReject = null;
    promise.catch(function () {
        log.push("reject");
        atReject = log.join(", ");
    });
    var error;
    try {
        await promise;
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), "Error: map");
    // Reject lands in the same turn as the map throw. An extra Await in
    // AsyncIteratorClose would let the next tick run first.
    shouldBe(atReject, "map, get return, 0, reject");
}

async function test() {
    for (var i = 0; i < 1e4; ++i)
        await orderOnce();

    await collectValues();
    await nextReadOnce();
    await throwingNextSkipsConstruct();
    await constructThrowSkipsClose();
    await nextThrowSkipsReturn();
    await mapThrowCloses();
    await successSkipsReturn();
    await nonObjectIterator();
    await nullPrototypeIterator();
    await generatorClose();
    await arrayLikeStillConstructsWithLength();

    for (var i = 0; i < 100; ++i)
        await closeDoesNotAddAnAwait();
}

test().catch(function (error) {
    print("FAIL");
    print(String(error));
    print(String(error.stack));
    $vm.abort();
});
drainMicrotasks();
