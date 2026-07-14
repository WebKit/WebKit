// op_async_iterator_open's fast path probes .next side-effect-free (PropertySlot VMInquiry). A genuine
// async generator whose .next is observable but not pristine (an instance accessor, or a Proxy on its
// prototype chain) must read .next exactly once -- via the generic getNext get_by_id, matching V8's single
// GetV(iterator, "next"). An observable probe would double the read or, without a VM, crash.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

const AsyncGeneratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf((async function* () {})()));
const pristineNext = AsyncGeneratorPrototype.next;

// Proxy on the prototype chain: the .next lookup is observable via a get trap.
async function proxyPrototypeNext() {
    let nextTrap = 0;
    function customNext(...args) { return pristineNext.apply(this, args); }
    const proxyProto = new Proxy(AsyncGeneratorPrototype, {
        get(target, prop, receiver) {
            if (prop === "next") { nextTrap++; return customNext; }
            return Reflect.get(target, prop, receiver);
        }
    });
    const g = (async function* () { yield 1; yield 2; yield 3; })();
    Object.setPrototypeOf(g, proxyProto);
    let sum = 0;
    for await (const x of g)
        sum += x;
    assert(sum === 6, "proxyPrototypeNext sum=" + sum);
    assert(nextTrap === 1, "proxyPrototypeNext .next read " + nextTrap + " times (expected exactly 1)");
}

// Accessor .next on the instance: non-pristine, read once; also guards the VMInquiry slot construction.
async function instanceAccessorNext() {
    let getterCount = 0;
    const g = (async function* () { yield 10; yield 20; })();
    Object.defineProperty(g, "next", { configurable: true, get() { getterCount++; return pristineNext; } });
    let sum = 0;
    for await (const x of g)
        sum += x;
    assert(sum === 30, "instanceAccessorNext sum=" + sum);
    assert(getterCount === 1, "instanceAccessorNext .next getter invoked " + getterCount + " times (expected exactly 1)");
}

// A single for-await site fed both pristine and observable generators: pristine ones make it fastEligible
// (and tier up), so the observable ones exercise the DFG/FTL fast-path fallthrough (one GetById of .next
// whose value is both compared against the pristine sentinel and committed). Still exactly one read.
async function fastEligibleFallthrough(observable) {
    let sum = 0;
    for await (const x of observable)
        sum += x;
    return sum;
}

function proxyGenerator(counter) {
    function customNext(...args) { return pristineNext.apply(this, args); }
    const proxyProto = new Proxy(AsyncGeneratorPrototype, {
        get(target, prop, receiver) {
            if (prop === "next") { counter.reads++; return customNext; }
            return Reflect.get(target, prop, receiver);
        }
    });
    const g = (async function* () { yield 4; yield 5; })();
    Object.setPrototypeOf(g, proxyProto);
    return g;
}

function accessorGenerator(counter) {
    const g = (async function* () { yield 7; })();
    Object.defineProperty(g, "next", { configurable: true, get() { counter.reads++; return pristineNext; } });
    return g;
}

let done = false;
let error = null;

async function main() {
    for (let i = 0; i < testLoopCount; i++) {
        await proxyPrototypeNext();
        await instanceAccessorNext();

        assert(await fastEligibleFallthrough((async function* () { yield 1; yield 2; })()) === 3, "pristine fallthrough sum");
        const c1 = { reads: 0 };
        assert(await fastEligibleFallthrough(proxyGenerator(c1)) === 9, "proxy fallthrough sum");
        assert(c1.reads === 1, "proxy fallthrough .next read " + c1.reads + " times (expected exactly 1)");
        const c2 = { reads: 0 };
        assert(await fastEligibleFallthrough(accessorGenerator(c2)) === 7, "accessor fallthrough sum");
        assert(c2.reads === 1, "accessor fallthrough .next getter invoked " + c2.reads + " times (expected exactly 1)");
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
