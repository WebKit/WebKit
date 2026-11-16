function assert(b, m) {
    if (!b)
        throw new Error(m);
}

const hop = Object.prototype.hasOwnProperty;

function oracle(o, p) {
    return Reflect.getOwnPropertyDescriptor(Object(o), p) !== undefined;
}
noInline(oracle);

// The guard must observe a delete of the current key.
function deleteCurrent(o) {
    const seen = [];
    for (const p in o) {
        delete o[p];
        seen.push(p + ':' + Object.prototype.hasOwnProperty.call(o, p));
    }
    return seen.join(',');
}
noInline(deleteCurrent);

function deleteCurrentHasOwn(o) {
    const seen = [];
    for (const p in o) {
        delete o[p];
        seen.push(p + ':' + Object.hasOwn(o, p));
    }
    return seen.join(',');
}
noInline(deleteCurrentHasOwn);

// A call that clobbers the world between the property name production and the guard.
let clobberTarget = null;
let clobberKey = '';
function clobber() {
    if (clobberTarget)
        delete clobberTarget[clobberKey];
}
noInline(clobber);

function guardAfterClobber(o, victim) {
    const seen = [];
    for (const p in o) {
        clobberTarget = p === victim ? o : null;
        clobberKey = p;
        clobber();
        seen.push(p + ':' + hop.call(o, p));
    }
    clobberTarget = null;
    return seen.join(',');
}
noInline(guardAfterClobber);

// The guard must always agree with Reflect.getOwnPropertyDescriptor, even while
// the mutator churns structures, prototypes, and upcoming keys.
function differential(o, mutate) {
    let ones = 0;
    for (const p in o) {
        if (mutate)
            mutate(o, p);
        const got = Object.prototype.hasOwnProperty.call(o, p);
        assert(got === oracle(o, p), "differential mismatch for " + p);
        if (got)
            ones++;
    }
    return ones;
}
noInline(differential);

// Loop variable reassignment: q keeps the enumerated name, p does not.
function reassign(o) {
    let n = 0;
    for (let p in o) {
        const q = p;
        p = 'not a property';
        assert(!hop.call(o, p), "reassigned key must miss");
        if (hop.call(o, q))
            n++;
    }
    return n;
}
noInline(reassign);

// Nested enumerations over the same object, guarding both loop variables.
function nested(o) {
    let n = 0;
    for (const p in o) {
        for (const q in o) {
            if (hop.call(o, p))
                n++;
            if (Object.hasOwn(o, q))
                n += 10;
        }
    }
    return n;
}
noInline(nested);

// The guard on a proxy must consult the getOwnPropertyDescriptor trap every time.
function proxyGuard(o, counter) {
    let n = 0;
    for (const p in o) {
        const before = counter.count;
        if (hop.call(o, p))
            n++;
        assert(counter.count > before, "guard must hit the proxy trap");
    }
    return n;
}
noInline(proxyGuard);

// A trap that starts throwing mid-enumeration must surface the exception.
function proxyThrow(o, state) {
    const seen = [];
    for (const p in o) {
        seen.push(p + ':' + hop.call(o, p));
        state.throwing = true;
    }
    return seen.join(',');
}
noInline(proxyThrow);

// Polymorphic bases: plain, null-proto, frozen, large, array, string primitive.
function polyBase(o) {
    let n = 0;
    for (const p in o) {
        if (Object.prototype.hasOwnProperty.call(o, p))
            n++;
    }
    return n;
}
noInline(polyBase);

const protoBase = { pa: 1, pb: 2 };

function makeLarge() {
    const o = {};
    for (let i = 0; i < 40; ++i)
        o['k' + i] = i;
    return o;
}

const largeTemplate = makeLarge();

for (let i = 0; i < testLoopCount; ++i) {
    assert(deleteCurrent({ a: 1, b: 2, c: 3 }) === 'a:false,b:false,c:false', "deleteCurrent");
    assert(deleteCurrentHasOwn({ a: 1, b: 2, c: 3 }) === 'a:false,b:false,c:false', "deleteCurrentHasOwn");

    assert(guardAfterClobber({ a: 1, b: 2, c: 3 }, 'b') === 'a:true,b:false,c:true', "guardAfterClobber");

    {
        const o = Object.create(protoBase);
        o.a = 1;
        o.b = 2;
        assert(differential(o, null) === 2, "differential proto");
    }
    {
        const o = { a: 1, b: 2, c: 3 };
        assert(differential(o, (t, p) => { if (p === 'a') delete t.c; }) === 2, "differential delete upcoming");
    }
    {
        const o = { a: 1, b: 2 };
        differential(o, (t, p) => { t['n' + p] = 1; });
    }
    {
        const o = Object.create(protoBase);
        o.a = 1;
        differential(o, (t, p) => { if (p === 'a') Object.setPrototypeOf(t, { zz: 1 }); });
    }
    {
        const o = { a: 1, b: 2, c: 3, d: 4 };
        delete o.b;
        o['k' + (i & 7)] = 1;
        assert(differential(o, null) === 4, "differential dictionary");
    }

    assert(reassign({ a: 1, b: 2, c: 3 }) === 3, "reassign");
    assert(nested({ a: 1, b: 2, c: 3 }) === 99, "nested");

    {
        const counter = { count: 0 };
        const proxy = new Proxy({ a: 1, b: 2 }, {
            getOwnPropertyDescriptor(t, k) {
                counter.count++;
                return Reflect.getOwnPropertyDescriptor(t, k);
            }
        });
        assert(proxyGuard(proxy, counter) === 2, "proxyGuard");
    }
    {
        const state = { throwing: false };
        const proxy = new Proxy({ a: 1, b: 2 }, {
            getOwnPropertyDescriptor(t, k) {
                if (state.throwing)
                    throw new Error('trap');
                return Reflect.getOwnPropertyDescriptor(t, k);
            }
        });
        let threw = false;
        let result = null;
        try {
            result = proxyThrow(proxy, state);
        } catch (e) {
            threw = true;
            assert(e.message === 'trap', "proxyThrow message");
        }
        assert(threw || result === 'a:true,b:true', "proxyThrow " + result);
    }

    assert(polyBase({ a: 1, b: 2, c: 3 }) === 3, "polyBase object");
    assert(polyBase("abcd") === 4, "polyBase string");
    {
        const arr = [10, 20, 30];
        arr.named = 1;
        delete arr[1];
        assert(polyBase(arr) === 3, "polyBase array");
    }
    {
        const o = Object.create(null);
        o.x = 1;
        o.y = 2;
        assert(polyBase(o) === 2, "polyBase null proto");
    }
    assert(polyBase(Object.freeze({ f1: 1, f2: 2 })) === 2, "polyBase frozen");
    {
        const o = { ...largeTemplate };
        assert(polyBase(o) === 40, "polyBase large");
    }
    {
        const o = Object.create(protoBase);
        o.own = 1;
        assert(polyBase(o) === 1, "polyBase inherited");
    }
}
