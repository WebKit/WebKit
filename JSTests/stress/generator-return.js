function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function shouldBeIteratorResult(actual, { value, done })
{
    shouldBe(actual.value, value);
    shouldBe(actual.done, done);
}

function unreachable()
{
    throw new Error('unreachable');
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

(function () {
    function *gen() {
        yield yield 20;
        yield 42;
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(0), { value: 20, done: false });
        shouldBeIteratorResult(g.return(20), { value: 20, done: true });
        shouldBeIteratorResult(g.return(20), { value: 20, done: true });
        shouldBeIteratorResult(g.next(42), { value: undefined, done: true });
    }
    {
        let g = gen();
        shouldBeIteratorResult(g.return(42), { value: 42, done: true });
        shouldBeIteratorResult(g.next(42), { value: undefined, done: true });
        shouldBeIteratorResult(g.return(42), { value: 42, done: true });
    }
}());

(function () {
    function *gen() {
        return 42;
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(), { value: 42, done: true });
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
    }
    {
        let g = gen();
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
        shouldBeIteratorResult(g.next(), { value: undefined, done: true });
        shouldBeIteratorResult(g.return(42), { value: 42, done: true });
    }
}());

(function () {
    function *gen() {
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(), { value: undefined, done: true });
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
    }
    {
        let g = gen();
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
        shouldBeIteratorResult(g.next(), { value: undefined, done: true });
        shouldBeIteratorResult(g.return(42), { value: 42, done: true });
    }
}());

(function () {
    function *gen() {
        try {
            yield 42;
        } finally {
            return 400;
        }
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(), { value: 42, done: false });
        shouldBeIteratorResult(g.return(0), { value: 400, done: true });
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
    }
    {
        let g = gen();
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
        shouldBeIteratorResult(g.next(), { value: undefined, done: true });
        shouldBeIteratorResult(g.return(42), { value: 42, done: true });
    }
}());


(function () {
    function *gen() {
        try {
            yield 42;
        } finally {
            throw new Error("thrown");
        }
    }

    {
        let g = gen();
        shouldBeIteratorResult(g.next(), { value: 42, done: false });
        shouldThrow(() => g.return(0), `Error: thrown`);
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
    }
    {
        let g = gen();
        shouldBeIteratorResult(g.return(0), { value: 0, done: true });
        shouldBeIteratorResult(g.next(), { value: undefined, done: true });
        shouldBeIteratorResult(g.return(42), { value: 42, done: true });
    }
}());
