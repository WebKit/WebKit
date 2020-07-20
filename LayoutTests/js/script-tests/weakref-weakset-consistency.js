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

let weakSet = new WeakSet();
let weakRefs = [];

async function* foo() {
    let weak = makeWeakRef();
    weakSet.add(weak.deref());
    await turnEventLoop();
    weakRefs.push(weak);
}

async function test() {
    for (let i = 0; i < loopCount; i++) {
        for await (value of foo()) { }
    }
    gc();

    if (weakRefs.find((weak) => weak.deref() && !weakSet.has(weak.deref())))
        throw new Error("Weak ref has target but weak set lost reference." )
    asyncTestPassed();
}

test().catch(e => debug(e));
