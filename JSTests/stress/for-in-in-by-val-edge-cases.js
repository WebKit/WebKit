function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function inHelper(o, p) {
    return p in o;
}

function oracle(o, p) {
    let cur = Object(o);
    while (cur) {
        if (Reflect.getOwnPropertyDescriptor(cur, p) !== undefined)
            return true;
        cur = Object.getPrototypeOf(cur);
    }
    return false;
}
noInline(oracle);

// The check must observe a delete of the current key.
function deleteCurrent(o) {
    const seen = [];
    for (const p in o) {
        delete o[p];
        seen.push(p + ':' + Reflect.has(o, p));
    }
    return seen.join(',');
}
noInline(deleteCurrent);

function deleteCurrentHelper(o) {
    const seen = [];
    for (const p in o) {
        delete o[p];
        seen.push(p + ':' + inHelper(o, p));
    }
    return seen.join(',');
}
noInline(deleteCurrentHelper);

// A call that clobbers the world between the property name production and the check.
let clobberTarget = null;
let clobberKey = '';
function clobber() {
    if (clobberTarget)
        delete clobberTarget[clobberKey];
}
noInline(clobber);

function checkAfterClobber(o, victim) {
    const seen = [];
    for (const p in o) {
        clobberTarget = p === victim ? o : null;
        clobberKey = p;
        clobber();
        seen.push(p + ':' + inHelper(o, p));
    }
    clobberTarget = null;
    return seen.join(',');
}
noInline(checkAfterClobber);

// The check must always agree with a prototype-chain walk over
// Reflect.getOwnPropertyDescriptor, even while the mutator churns structures,
// prototypes, and upcoming keys.
function differential(o, mutate) {
    let ones = 0;
    for (const p in o) {
        if (mutate)
            mutate(o, p);
        const got = Reflect.has(o, p);
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
        assert(!inHelper(o, p), "reassigned key must miss");
        if (inHelper(o, q))
            n++;
    }
    return n;
}
noInline(reassign);

// Nested enumerations over the same object, checking both loop variables.
function nested(o) {
    let n = 0;
    for (const p in o) {
        for (const q in o) {
            if (inHelper(o, p))
                n++;
            if (Reflect.has(o, q))
                n += 10;
        }
    }
    return n;
}
noInline(nested);

// The check on a proxy must consult the has trap every time.
function proxyCheck(o, counter) {
    let n = 0;
    for (const p in o) {
        const before = counter.count;
        if (inHelper(o, p))
            n++;
        assert(counter.count > before, "check must hit the proxy trap");
    }
    return n;
}
noInline(proxyCheck);

// A trap that starts throwing mid-enumeration must surface the exception.
function proxyThrow(o, state) {
    const seen = [];
    for (const p in o) {
        seen.push(p + ':' + inHelper(o, p));
        state.throwing = true;
    }
    return seen.join(',');
}
noInline(proxyThrow);

// Polymorphic bases: plain, null-proto, frozen, large, array, boxed string.
function polyBase(o) {
    let n = 0;
    for (const p in o) {
        if (Reflect.has(o, p))
            n++;
    }
    return n;
}
noInline(polyBase);

const protoBase = { pa: 1, pb: 2 };
const shadowProto = { a: 42 };

function makeLarge() {
    const o = {};
    for (let i = 0; i < 40; ++i)
        o['k' + i] = i;
    return o;
}

const largeTemplate = makeLarge();

for (let i = 0; i < testLoopCount; ++i) {
    assert(deleteCurrent({ a: 1, b: 2, c: 3 }) === 'a:false,b:false,c:false', "deleteCurrent");
    assert(deleteCurrentHelper({ a: 1, b: 2, c: 3 }) === 'a:false,b:false,c:false', "deleteCurrentHelper");

    {
        // Deleting the own property must still find it on the prototype chain.
        const o = Object.create(shadowProto);
        o.a = 1;
        o.b = 2;
        assert(deleteCurrentHelper(o) === 'a:true,b:false', "deleteCurrent shadow");
    }

    assert(checkAfterClobber({ a: 1, b: 2, c: 3 }, 'b') === 'a:true,b:false,c:true', "checkAfterClobber");

    {
        const o = Object.create(protoBase);
        o.a = 1;
        o.b = 2;
        // Own a, b plus inherited pa, pb are all present for `in`.
        assert(differential(o, null) === 4, "differential proto");
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
            has(t, k) {
                counter.count++;
                return Reflect.has(t, k);
            }
        });
        assert(proxyCheck(proxy, counter) === 2, "proxyCheck");
    }
    {
        const state = { throwing: false };
        const proxy = new Proxy({ a: 1, b: 2 }, {
            has(t, k) {
                if (state.throwing)
                    throw new Error('trap');
                return Reflect.has(t, k);
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
    assert(polyBase(Object("abcd")) === 4, "polyBase string");
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
        // for-in visits own + inherited; `in` is true for every visited key.
        assert(polyBase(o) === 3, "polyBase inherited");
    }
}
