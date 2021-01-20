//@ requireOptions("--useWeakRefs=true")

asyncTestStart(1);

function makeWeakRef() { return new WeakRef({foo: 1 }); }
noInline(makeWeakRef);

// Turns out the test times out if we turn the event loop 10000 times...
let loopCount = globalThis.window ? 300 : 10000;
function turnEventLoop() {
    return new Promise(function(resolve, reject) {
        if (globalThis.window) {
            window.setTimeout(() => {
                resolve();
            }, 0);
        } else {
            releaseWeakRefs();
            resolve();
        }
    });
}

let weakRefs = [];

async function* foo() {
    let weak = makeWeakRef();
    let object = weak.deref();
    yield await 1;
    if (weak.deref() !== object)
        throw new Error("Object appears to have been collected incorrectly");
    object = null;
    yield await turnEventLoop();
    weakRefs.push(weak);
}

async function test() {
    for (let i = 0; i < loopCount; i++) {
        for await (value of foo()) { }
    }
    gc();

    if (weakRefs.find((weak) => weak.deref() === null) === undefined)
        throw new Error("No weak ref value was collected");
    asyncTestPassed();
}


test().catch(e => debug(e));
