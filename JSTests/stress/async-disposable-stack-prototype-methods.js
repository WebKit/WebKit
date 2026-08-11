//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function shouldThrow(run, errorType, message) {
    let error;
    let threw = false;
    try {
        run();
    } catch (e) {
        threw = true;
        error = e;
    }
    if (!threw)
        throw new Error(`Expected to throw ${errorType.name}, but did not throw.`);
    if (!(error instanceof errorType))
        throw new Error(`Expected to throw ${errorType.name}, but threw '${error}'`);
    if (message !== void 0 && error.message !== message)
        throw new Error(`Expected to throw '${message}', but threw '${error.message}'`);
}

function shouldThrowAsync(run, errorType, message) {
    let actual;
    let hadError = false;
    run().then(v => { actual = v; },
               e => { hadError = true; actual = e; });
    drainMicrotasks();
    if (!hadError)
        throw new Error("Expected async throw, but promise fulfilled.");
    if (!(actual instanceof errorType))
        throw new Error(`Expected to throw ${errorType.name}, but threw '${actual}'`);
    if (message !== void 0 && actual.message !== message)
        throw new Error(`Expected to throw '${message}', but threw '${actual.message}'`);
}

/* adopt */

{
    const stack = new AsyncDisposableStack();
    const value = { x: 1 };
    let called = 0;
    let captured;
    shouldBe(stack.adopt(value, v => { called++; captured = v; }), value);
    const p = stack.disposeAsync();
    shouldBe(p instanceof Promise, true);
    drainMicrotasks();
    shouldBe(called, 1);
    shouldBe(captured, value);
}

{
    const stack = new AsyncDisposableStack();
    shouldThrow(() => stack.adopt(0, 1), TypeError);
}

{
    const stack = new AsyncDisposableStack();
    stack.disposeAsync();
    shouldThrow(() => stack.adopt({}, () => {}), ReferenceError);
}

/* defer */

{
    const stack = new AsyncDisposableStack();
    let called = 0;
    shouldBe(stack.defer(() => { called++; }), undefined);
    const p = stack.disposeAsync();
    shouldBe(p instanceof Promise, true);
    drainMicrotasks();
    shouldBe(called, 1);
}

{
    const stack = new AsyncDisposableStack();
    shouldThrow(() => stack.defer(1), TypeError);
}

{
    const stack = new AsyncDisposableStack();
    stack.disposeAsync();
    shouldThrow(() => stack.defer(() => {}), ReferenceError);
}

/* use */

{
    const stack = new AsyncDisposableStack();
    let disposed = 0;
    const resource = {
        async [Symbol.asyncDispose]() { disposed++; }
    };
    shouldBe(stack.use(resource), resource);
    const p = stack.disposeAsync();
    shouldBe(p instanceof Promise, true);
    drainMicrotasks();
    shouldBe(disposed, 1);
}

{
    const stack = new AsyncDisposableStack();
    shouldThrow(() => stack.use({}), TypeError);
}

{
    const stack = new AsyncDisposableStack();
    stack.disposeAsync();
    shouldThrow(() => stack.use({ async [Symbol.asyncDispose]() {} }), ReferenceError);
}

/* move */

{
    const stack = new AsyncDisposableStack();
    let disposed = 0;
    stack.defer(() => { disposed++; });
    const moved = stack.move();
    const p1 = stack.disposeAsync();
    let fulfilled1 = 0;
    p1.then(() => fulfilled1++);
    drainMicrotasks();
    shouldBe(fulfilled1, 1);
    shouldBe(disposed, 0);

    const p2 = moved.disposeAsync();
    p2.then(() => {});
    drainMicrotasks();
    shouldBe(disposed, 1);
}

{
    const stack = new AsyncDisposableStack();
    stack.disposeAsync();
    shouldThrow(() => stack.move(), ReferenceError);
}

/* disposeAsync */

{
    const stack = new AsyncDisposableStack();
    let fulfilled = 0;
    const p = stack.disposeAsync();
    p.then(v => { fulfilled++; shouldBe(v, undefined); });
    drainMicrotasks();
    shouldBe(fulfilled, 1);
}

{
    const stack = new AsyncDisposableStack();
    let first = 0;
    let second = 0;
    stack.disposeAsync().then(() => first++);
    drainMicrotasks();
    shouldBe(first, 1);
    stack.disposeAsync().then(() => second++);
    drainMicrotasks();
    shouldBe(second, 1);
}

{
    shouldThrowAsync(
        () => AsyncDisposableStack.prototype.disposeAsync.call({}),
        TypeError
    );
}

{
    const order = [];
    const stack = new AsyncDisposableStack();
    stack.defer(() => order.push(1));
    stack.defer(() => order.push(2));
    stack.disposeAsync();
    drainMicrotasks();
    shouldBe(order.join(','), '2,1');
}

{
    const order = [];
    let resolver;
    const stack = new AsyncDisposableStack();
    stack.defer(() => order.push('b'));
    stack.defer(() => {
        order.push('a-start');
        return new Promise(r => { resolver = () => { order.push('a-end'); r(); }; });
    });
    let fulfilled = 0;
    const p = stack.disposeAsync();
    p.then(() => fulfilled++);
    drainMicrotasks();
    shouldBe(order.join(','), 'a-start');
    shouldBe(fulfilled, 0);
    resolver();
    drainMicrotasks();
    shouldBe(order.join(','), 'a-start,a-end,b');
    shouldBe(fulfilled, 1);
}

{
    function E1() {}
    const stack = new AsyncDisposableStack();
    stack.defer(() => {});
    stack.defer(() => Promise.reject(new E1()));
    shouldThrowAsync(() => stack.disposeAsync(), E1);
}

{
    const stack = new AsyncDisposableStack();
    function E1() {}
    function E2() {}
    stack.defer(() => { throw new E1(); });
    stack.defer(() => { throw new E2(); });
    shouldThrowAsync(() => stack.disposeAsync(), SuppressedError);
}

{
    function E1() {}
    function E2() {}
    const stack = new AsyncDisposableStack();
    stack.defer(() => { throw new E2(); });
    stack.defer(() => Promise.reject(new E1()));
    shouldThrowAsync(() => stack.disposeAsync(), SuppressedError);
}
