//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${JSON.stringify(b)} but got ${JSON.stringify(a)}`);
}

// AsyncDisposableStack.prototype.adopt step 6: the closure returns the result
// of Call(onDisposeAsync, undefined, « value »). disposeAsync must await that
// result. If the closure swallows the return value, disposeAsync resolves
// before the onDisposeAsync promise settles.

// Ordering: disposeAsync must not resolve before onDisposeAsync's promise.
{
    const events = [];
    const stack = new AsyncDisposableStack();
    const slow = () => Promise.resolve()
        .then(() => {})
        .then(() => {})
        .then(() => {})
        .then(() => {})
        .then(() => events.push("slow-done"));

    stack.adopt("R", () => {
        events.push("adopt-called");
        return slow();
    });
    stack.disposeAsync().then(() => events.push("dispose-resolved"));
    drainMicrotasks();

    shouldBe(events.join(","), "adopt-called,slow-done,dispose-resolved");
}

// onDisposeAsync returns a rejected promise → SuppressedError chain works,
// and disposeAsync still waits for it.
{
    const events = [];
    const stack = new AsyncDisposableStack();
    stack.adopt("A", () => {
        events.push("A-called");
        return Promise.resolve().then(() => {}).then(() => { throw new Error("A-fail"); });
    });
    stack.adopt("B", () => {
        events.push("B-called");
        return Promise.resolve().then(() => {}).then(() => { events.push("B-done"); });
    });
    let caught;
    stack.disposeAsync().then(
        () => events.push("resolved"),
        (e) => { events.push("rejected"); caught = e; }
    );
    drainMicrotasks();

    // LIFO: B first (succeeds), then A (fails). A's rejection is the only error.
    shouldBe(events.join(","), "B-called,B-done,A-called,rejected");
    shouldBe(caught.message, "A-fail");
}

// onDisposeAsync returning a thenable (not a real Promise)
{
    const events = [];
    const stack = new AsyncDisposableStack();
    stack.adopt("R", () => {
        events.push("called");
        return {
            then(onFulfilled) {
                Promise.resolve().then(() => {}).then(() => {
                    events.push("thenable-resolved");
                    onFulfilled();
                });
            }
        };
    });
    stack.disposeAsync().then(() => events.push("dispose-resolved"));
    drainMicrotasks();
    shouldBe(events.join(","), "called,thenable-resolved,dispose-resolved");
}

// onDisposeAsync returning a non-promise primitive → resolved immediately
{
    const events = [];
    const stack = new AsyncDisposableStack();
    stack.adopt("R", (v) => {
        events.push("called:" + v);
        return 42;
    });
    stack.disposeAsync().then(() => events.push("resolved"));
    drainMicrotasks();
    shouldBe(events.join(","), "called:R,resolved");
}
