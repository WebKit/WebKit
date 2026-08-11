function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

// An unhandled rejected promise is pinned only by a Vector<Strong<JSPromise>> in
// VM (m_aboutToBeNotifiedRejectedPromises) until the rejection tracker fires, so
// with Strong<> marking broken every WeakRef below would go empty. The count spans
// multiple StrongBlocks.
const promiseCount = 2200;
const weakRefs = [];

for (let i = 0; i < promiseCount; ++i)
    weakRefs.push(new WeakRef(Promise.reject(new Error("strong-handle-gc " + i))));

// Churn the stack so conservative scanning cannot keep the promises alive via
// stale pointers left in registers or on the native stack.
function churn(depth) {
    if (depth <= 0)
        return 0;
    let sum = 0;
    const objects = [];
    for (let i = 0; i < 1000; ++i)
        objects.push({ a: i, b: i * 2, c: [i, i + 1] });
    for (let i = 0; i < objects.length; ++i)
        sum += objects[i].a + objects[i].b;
    return sum + churn(depth - 1);
}

for (let round = 0; round < 3; ++round) {
    churn(4);
    // Drop the "kept alive until end of turn" list, so this observes only the
    // Strong<> root.
    $.clearKeptObjects();
    $vm.gc();
}

for (let i = 0; i < promiseCount; ++i)
    shouldBe(weakRefs[i].deref() !== undefined, true);
