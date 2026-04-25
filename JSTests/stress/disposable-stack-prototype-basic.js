//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType, message) {
    let error;
    try { func(); } catch (e) { error = e; }
    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
    if (message !== undefined)
        shouldBe(String(error), message);
}

// DisposableStack.prototype.adopt
{
    let called = false;
    const value = { foo: 1 };
    const stack = new DisposableStack();
    const ret = stack.adopt(value, v => { shouldBe(v, value); called = true; });
    shouldBe(ret, value);
    stack.dispose();
    shouldBe(called, true);
}
{
    const stack = new DisposableStack();
    shouldThrow(() => stack.adopt({}, 1), TypeError);
}
{
    const stack = new DisposableStack();
    stack.dispose();
    shouldThrow(() => stack.adopt({}, () => {}), ReferenceError);
}
{
    shouldThrow(() => DisposableStack.prototype.adopt.call({}, {}, () => {}), TypeError);
}

// DisposableStack.prototype.defer
{
    const order = [];
    const stack = new DisposableStack();
    stack.defer(() => order.push(1));
    stack.defer(() => order.push(2));
    stack.dispose();
    shouldBe(order.join(","), "2,1");
}
{
    const stack = new DisposableStack();
    shouldThrow(() => stack.defer(0), TypeError);
}
{
    const stack = new DisposableStack();
    stack.dispose();
    shouldThrow(() => stack.defer(() => {}), ReferenceError);
}

// DisposableStack.prototype.dispose
{
    let count = 0;
    const stack = new DisposableStack();
    stack.defer(() => ++count);
    stack.dispose();
    stack.dispose();
    shouldBe(count, 1);
}
{
    const stack = new DisposableStack();
    stack.defer(() => { throw new Error("first"); });
    stack.defer(() => { throw new Error("second"); });
    shouldThrow(() => stack.dispose(), SuppressedError);
}
{
    const stack = new DisposableStack();
    stack.defer(() => { throw new Error("only"); });
    shouldThrow(() => stack.dispose(), Error, "Error: only");
}
{
    const stack = new DisposableStack();
    shouldBe(stack.dispose(), undefined);
}

// DisposableStack.prototype.use
{
    const res = {
        disposed: false,
        [Symbol.dispose]() { this.disposed = true; }
    };
    const stack = new DisposableStack();
    const ret = stack.use(res);
    shouldBe(ret, res);
    stack.dispose();
    shouldBe(res.disposed, true);
}
{
    const order = [];
    const stack = new DisposableStack();
    const a = { [Symbol.dispose]() { order.push("use1"); } };
    const b = { [Symbol.dispose]() { order.push("use2"); } };
    stack.use(a);
    stack.defer(() => order.push("defer"));
    stack.adopt({}, () => order.push("adopt"));
    stack.use(b);
    stack.dispose();
    shouldBe(order.join(","), "use2,adopt,defer,use1");
}
{
    const stack = new DisposableStack();
    shouldThrow(() => stack.use(42), TypeError);
}
{
    const stack = new DisposableStack();
    stack.dispose();
    shouldThrow(() => stack.use({}), ReferenceError);
}
{
    const stack = new DisposableStack();
    stack.use(null);
    stack.use(undefined);
    stack.dispose();
    shouldBe(stack.disposed, true);
}

// DisposableStack.prototype.move
{
    let called = false;
    const stack1 = new DisposableStack();
    stack1.defer(() => called = true);
    const stack2 = stack1.move();
    shouldThrow(() => stack1.defer(() => {}), ReferenceError);
    stack2.dispose();
    shouldBe(called, true);
    shouldBe(stack1.disposed, true);
}
{
    const stack = new DisposableStack();
    stack.dispose();
    shouldThrow(() => stack.move(), ReferenceError);
}
{
    shouldThrow(() => DisposableStack.prototype.move.call({}), TypeError);
}
{
    const order = [];
    const s1 = new DisposableStack();
    s1.adopt("v", () => order.push("adopt"));
    s1.defer(() => order.push("defer"));
    s1.use({ [Symbol.dispose]() { order.push("use"); } });
    const s2 = s1.move();
    s2.dispose();
    shouldBe(order.join(","), "use,defer,adopt");
}
