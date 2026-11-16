function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

class DerivedPromise extends Promise { }

{
    let p = Promise.resolve(42);
    let result = DerivedPromise.resolve(p);

    shouldBe(result === p, false);
    shouldBe(result instanceof DerivedPromise, true);
    shouldBe(Object.getPrototypeOf(result), DerivedPromise.prototype);

    let resolvedValue;
    result.then((v) => { resolvedValue = v; });
    drainMicrotasks();
    shouldBe(resolvedValue, 42);
}

{
    let dp = DerivedPromise.resolve(1);
    shouldBe(DerivedPromise.resolve(dp) === dp, true);
}

{
    let p = Promise.resolve(1);
    shouldBe(Promise.resolve(p) === p, true);
}

{
    let capturedResolve;
    function MyThenable(executor) {
        executor((v) => { capturedResolve = v; }, () => { });
        this.then = () => { };
    }

    let p = Promise.resolve(99);
    let result = Promise.resolve.call(MyThenable, p);

    shouldBe(result === p, false);
    shouldBe(result instanceof MyThenable, true);
    shouldBe(capturedResolve, p);
}
