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

class CallSite {
    constructor()
    {
        this.count = 0;
    }

    call()
    {
        return this.count++;
    }
}

(function () {
    class Arrays {
        constructor()
        {
            this.first = [ 0, 1, 2, 3 ];
            this.second = [ 4, 5, 6, 7 ];
        }

        *[Symbol.iterator]()
        {
            yield * this.first;
            yield * this.second;
        }
    }

    var arrays = new Arrays;
    let i = 0;
    for (let value of arrays)
        shouldBe(i++, value);
}());

(function () {
    // throw should be propagated.
    let c1 = new CallSite;
    class Iterator {
        next(value)
        {
            return { value, done: false };
        }

        'throw'(value) {
            shouldBe(value, 42);
            c1.call();
            throw new Error("OK");
        }

        [Symbol.iterator]()
        {
            return this;
        }
    }

    function *gen()
    {
        let iter = new Iterator();
        yield * iter;
    }

    let g = gen();
    shouldBe(g.next(0).value, undefined);
    shouldBe(g.next(1).value, 1);
    shouldBe(g.next(2).value, 2);
    shouldThrow(() => {
        g.throw(42);
    }, `Error: OK`);
    shouldThrow(() => {
        g.throw(44);
    }, `44`);
    shouldBe(c1.count, 1);
}());

(function () {
    // No `throw` method.
    let c1 = new CallSite;
    class Iterator {
        next(value)
        {
            return { value, done: false };
        }

        'return'(value)
        {
            shouldBe(value, undefined);
            c1.call();
            return { value, done: true };
        }

        [Symbol.iterator]()
        {
            return this;
        }
    }

    function *gen()
    {
        let iter = new Iterator();
        yield * iter;
    }

    let g = gen();
    shouldBe(g.next(0).value, undefined);
    shouldBe(g.next(1).value, 1);
    shouldBe(g.next(2).value, 2);
    shouldThrow(() => {
        g.throw(42);
    }, `TypeError: Delegated generator does not have a 'throw' method.`);
    shouldThrow(() => {
        g.throw(44);
    }, `44`);
    shouldBe(c1.count, 1);
}());

(function () {
    // No `throw` method, `return` returns an incorrect result.
    let c1 = new CallSite;
    class Iterator {
        next(value)
        {
            return { value, done: false };
        }

        'return'(value)
        {
            shouldBe(value, undefined);
            c1.call();
        }

        [Symbol.iterator]()
        {
            return this;
        }
    }

    function *gen()
    {
        let iter = new Iterator();
        yield * iter;
    }

    let g = gen();
    shouldBe(g.next(0).value, undefined);
    shouldBe(g.next(1).value, 1);
    shouldBe(g.next(2).value, 2);
    shouldThrow(() => {
        g.throw(42);
    }, `TypeError: Iterator result interface is not an object.`);
    shouldThrow(() => {
        g.throw(44);
    }, `44`);
    shouldBe(c1.count, 1);
}());

(function () {
    // No `throw` method, No `return` method.
    class Iterator {
        next(value)
        {
            return { value, done: false };
        }

        [Symbol.iterator]()
        {
            return this;
        }
    }

    function *gen()
    {
        let iter = new Iterator();
        yield * iter;
    }

    let g = gen();
    shouldBe(g.next(0).value, undefined);
    shouldBe(g.next(1).value, 1);
    shouldBe(g.next(2).value, 2);
    shouldThrow(() => {
        g.throw(42);
    }, `TypeError: Delegated generator does not have a 'throw' method.`);
    shouldThrow(() => {
        g.throw(44);
    }, `44`);
}());


(function () {
    // `throw` does not throw. Not returns a object.
    class Iterator {
        next(value)
        {
            return { value, done: false };
        }

        'throw'(value)
        {
        }

        [Symbol.iterator]()
        {
            return this;
        }
    }

    function *gen()
    {
        let iter = new Iterator();
        yield * iter;
    }

    let g = gen();
    shouldBe(g.next(0).value, undefined);
    shouldBe(g.next(1).value, 1);
    shouldBe(g.next(2).value, 2);
    shouldThrow(() => {
        g.throw(42);
    }, `TypeError: Iterator result interface is not an object.`);
    shouldThrow(() => {
        g.throw(44);
    }, `44`);
}());

(function () {
    // `throw` does not throw. If returned iterator result is marked as done, it becomes `return`.
    class Iterator {
        next(value)
        {
            return { value, done: false };
        }

        'throw'(value)
        {
            return { value, done: true };
        }

        [Symbol.iterator]()
        {
            return this;
        }
    }

    function *gen()
    {
        let iter = new Iterator();
        let result = yield * iter;
        shouldBe(result, 42);
        yield 21;
    }

    let g = gen();
    shouldBe(g.next(0).value, undefined);
    shouldBe(g.next(1).value, 1);
    shouldBe(g.next(2).value, 2);
    shouldBe(g.throw(42).value, 21);
    shouldBe(g.next().done, true);
    shouldThrow(() => {
        g.throw(44);
    }, `44`);
}());

(function () {
    // `return` returns done: false.
    class Iterator {
        next(value)
        {
            return { value, done: false };
        }

        'return'(value)
        {
            return { value, done: false };
        }

        [Symbol.iterator]()
        {
            return this;
        }
    }

    function *gen()
    {
        let iter = new Iterator();
        let result = yield * iter;
        yield result;
        yield 42;
    }

    let g = gen();
    shouldBe(g.next(0).value, undefined);
    shouldBe(g.next(1).value, 1);
    shouldBe(g.next(2).value, 2);
    shouldBe(g.return(42).value, 42);
    shouldBe(g.return(42).done, false);
}());

(function () {
    function *gen()
    {
        let result = yield * [ 0, 1, 2 ];
        yield result;
    }

    let g = gen();
    shouldBe(g.next().value, 0);
    shouldBe(g.next().value, 1);
    shouldBe(g.next().value, 2);
    shouldBe(g.next().value, undefined);
}());
