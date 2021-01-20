//@ requireOptions("--useWeakRefs=true")

asyncTestStart(1);

function makeWeakRef() { return new WeakRef({foo: 1}); }
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

let weakRefs = []

async function test() {

    for (let i = 0; i < loopCount; i++) {
        let weak = makeWeakRef();
        if (weak.deref().foo != 1)
            throw new Error("weak ref was freed too early.");
        weakRefs.push(weak);
    }
    await turnEventLoop();
    gc();

    if (!weakRefs.find((weak) => weak.deref() === null))
        throw new Error("no weak ref was collected");
    asyncTestPassed();
}

test().catch(e => debug(e));
