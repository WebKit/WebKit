function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function shouldBeIteratorResult(actual, { value, done })
{
    shouldBe(actual.value, value);
    shouldBe(actual.done, done);
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
    function *gen() {
        yield yield 20;
        yield 42;
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(0), { value: 20, done: false });
        shouldThrow(() => g.throw(20), `20`);
        shouldThrow(() => g.throw(20), `20`);
        shouldBeIteratorResult(g.next(42), { value: undefined, done: true });
        shouldBeIteratorResult(g.return(42), { value: 42, done: true });
    }
    {
        let g = gen();
        shouldThrow(() => g.throw(42), `42`);
        shouldBeIteratorResult(g.next(42), { value: undefined, done: true });
        shouldBeIteratorResult(g.return(42), { value: 42, done: true });
        shouldThrow(() => g.throw(42), `42`);
    }
}());

(function () {
    function *gen() {
        return 42;
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(), { value: 42, done: true });
        shouldThrow(() => g.throw(0), `0`);
    }
    {
        let g = gen();
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
        shouldBeIteratorResult(g.next(), { value: undefined, done: true });
        shouldThrow(() => g.throw(42), `42`);
    }
}());

(function () {
    function *gen() {
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(), { value: undefined, done: true });
        shouldThrow(() => g.throw(0), `0`);
    }
    {
        let g = gen();
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
        shouldBeIteratorResult(g.next(), { value: undefined, done: true });
        shouldThrow(() => g.throw(42), `42`);
    }
}());

(function () {
    let site = new CallSite();
    function *gen() {
        try {
            yield 42;
        } catch (e) {
            shouldBe(e, 0);
            site.call();
        }
        return 42;
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(), { value: 42, done: false });
        shouldBeIteratorResult(g.throw(0), { value: 42, done: true });
        shouldBe(site.count, 1);
    }
}());

(function () {
    function *gen() {
        try {
            yield 42;
        } finally {
            return 42;
        }
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(), { value: 42, done: false });
        shouldBeIteratorResult(g.throw(0), { value: 42, done: true });
        shouldThrow(() => g.throw(0), `0`);
    }
}());
