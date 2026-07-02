//@ requireOptions("--useConcurrentJIT=0")

function assert(b, msg) {
    if (!b)
        throw new Error("FAIL: " + msg);
}

function assertThrows(fn, msg) {
    let threw = false;
    try { fn(); } catch (e) { threw = e instanceof TypeError; }
    assert(threw, msg);
}

// Test 1: CSE of redundant CheckPrivateBrand in polymorphic case.
// With 8 classes, ByteCodeParser keeps the raw CheckPrivateBrand node
// instead of lowering to CheckStructure, so the new clobberize rule
// and def(HeapLocation) are exercised.
(function testPolymorphicCSE() {
    function factory(v) {
        return class {
            #m() { return v; }
            go() { return this.#m() + this.#m() + this.#m(); }
        };
    }
    const objs = [];
    for (let i = 0; i < 8; i++)
        objs.push(new (factory(i))());

    function run(objs) {
        let sum = 0;
        for (let j = 0; j < objs.length; j++)
            sum += objs[j].go();
        return sum;
    }
    noInline(run);

    for (let i = 0; i < testLoopCount; i++)
        assert(run(objs) === 84, "polymorphic CSE sum");
})();

// Test 2: CellUse speculation must preserve TypeError for non-cell bases.
// After CellUse OSR exit, baseline must still throw.
(function testCellUseThrow() {
    class C {
        #m() { return 1; }
        static call(o) { return o.#m(); }
    }
    noInline(C.call);

    const c = new C();
    for (let i = 0; i < testLoopCount; i++)
        assert(C.call(c) === 1, "warmup");

    assertThrows(() => C.call(42), "number base must throw");
    assertThrows(() => C.call("str"), "string base must throw");
    assertThrows(() => C.call(true), "boolean base must throw");
    assertThrows(() => C.call(null), "null base must throw");
    assertThrows(() => C.call(undefined), "undefined base must throw");
    assertThrows(() => C.call(Symbol()), "symbol base must throw");
})();

// Test 3: CheckPrivateBrand must NOT be CSE'd across a structure transition.
// The clobberize rule read(JSCell_structureID) means a write to structureID
// in between should invalidate CSE. A property store transitions the structure.
(function testNoStaleCSEAcrossTransition() {
    function factory() {
        return class {
            #m() { return this.x || 0; }
            go() {
                let a = this.#m();
                this.x = 42;
                let b = this.#m();
                return a + b;
            }
        };
    }
    const objs = [];
    for (let i = 0; i < 8; i++)
        objs.push(new (factory())());

    function run(objs) {
        let sum = 0;
        for (let j = 0; j < objs.length; j++) {
            delete objs[j].x;
            sum += objs[j].go();
        }
        return sum;
    }
    noInline(run);

    for (let i = 0; i < testLoopCount; i++)
        assert(run(objs) === 336, "brand check across transition");
})();

// Test 4: Brand check must still fail for wrong-class objects after CSE warmup.
(function testWrongBrandAfterCSE() {
    function factory() {
        return class {
            #m() { return 1; }
            go(other) { return this.#m() + other.#m(); }
        };
    }
    const A = factory();
    const B = factory();
    const a = new A();
    const b = new B();

    function run(x, y) { return x.go(y); }
    noInline(run);

    for (let i = 0; i < testLoopCount; i++)
        assert(run(a, a) === 2, "same brand warmup");

    assertThrows(() => run(a, b), "cross-brand must throw after CSE warmup");
})();

// Test 5: LICM safety - brand check hoisted out of loop must still be correct.
(function testLICM() {
    function factory(v) {
        return class {
            #m() { return v; }
            sum(n) {
                let s = 0;
                for (let i = 0; i < n; i++)
                    s += this.#m();
                return s;
            }
        };
    }
    const objs = [];
    for (let i = 0; i < 8; i++)
        objs.push(new (factory(i + 1))());

    function run(objs) {
        let sum = 0;
        for (let j = 0; j < objs.length; j++)
            sum += objs[j].sum(10);
        return sum;
    }
    noInline(run);

    for (let i = 0; i < testLoopCount; i++)
        assert(run(objs) === 360, "LICM sum");
})();
