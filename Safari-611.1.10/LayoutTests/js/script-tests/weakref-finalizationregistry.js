//@ requireOptions("--useWeakRefs=true")

asyncTestStart(1);
var objects = [];
var weakRefs = [];
var finalizerCalled = false;
var finalizationRegistry = new FinalizationRegistry(() => finalizerCalled = true);
function makeWeakRef() { return new WeakRef({ foo: 1 }); }
noInline(makeWeakRef);

// At the time of writing this test standalone-pre.js prints newlines slightly differently from js-test-pre.js for async tests so we don't use the shouldBe helpers...
function assert(condition, msg = "") {
    if (!condition)
        throw new Error(msg);
}

let loopCount = 1000;
function turnEventLoop() {
    return new Promise(function(resolve) {
        setTimeout(() => {
            gc();
            resolve();
        }, 0);
    });
}

var i;
async function test() {
    for (let i = 0; i < loopCount; i++) {
        let weak = makeWeakRef();
        weakRefs.push(weak);
        objects.push(weak.deref());
        finalizationRegistry.register(weak.deref());
    }

    await turnEventLoop();

    assert(finalizerCalled === false);
    for (i = 0; i < loopCount; i++)
        assert(weakRefs[i].deref() === objects[i], "failed on iteration: " + i);

    objects.length = 0;
    objects = null;

    await turnEventLoop();
    // We need to turn the event loop again since FR may not have called the callback in the last turn.
    await turnEventLoop();

    assert(finalizerCalled === true);
    assert(weakRefs.some((weakRef) => weakRef.deref() === null) === true);

    asyncTestPassed();
}

test().catch(e => debug(e));
