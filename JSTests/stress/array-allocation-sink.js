function assert(actual, expected) {
    for (let i = 0; i < actual.length; i++) {
        if (actual[i] != expected[i])
            throw new Error("bad actual=" + actual[i] + " but expected=" + expected[i]);
    }
}

function run(func, a) {
    let expected;
    for (let i = 0; i < testLoopCount; i++) {
        if (a == undefined)
            a = [1, 2];
        let res = func(a);
        if (i == 0)
            expected = res;
        assert(res, expected);
    }
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | sink
        a[0] = 43; // ArrayWithInt32
        s[0] = a[0];

        var q = { f: s[1] ? a : 42 }; // MaterializeNewArrayWithConstantSize
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | sink
        a[0] = s[1] & 0x8; // ArrayWithInt32
        s[0] = a[0];

        var q = { f: s[1] ? a : 42 }; // MaterializeNewArrayWithConstantSize
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | sink
        a[0] = s[1]; // ArrayWithContiguous
        s[0] = a[0];

        var q = { f: s[1] ? a : 42 }; // MaterializeNewArrayWithConstantSize
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | sink
        a[0] = s[1]; // ArrayWithDouble
        s[0] = a[0];

        var q = { f: s[1] ? a : 42 }; // MaterializeNewArrayWithConstantSize
        return s;
    }
    noInline(test);
    run(test, [0.1, 0.2]);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | sink
        a[0] = { f: 10 }; // ArrayWithContiguous
        s[0] = a[0];

        var q = { f: s[1] ? a : 42 }; // MaterializeNewArrayWithConstantSize
        return s;
    }
    noInline(test);
    run(test, [0.1, 0.2]);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | no sink
        a[0] = 42;
        a.length = 10; // escape
        s[0] = a[0];
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | sink
        a[0] = 42;
        s[1] = a.length;
        s[0] = a[0];
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | sink
        a[0] = 42;
        s[0] = a[0];
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        Array[Symbol.species] = 99; // FIXME: Maybe we should not sink
        let a = new Array(4); // NewArrayWithConstantSize | sink
        a[0] = 42;
        s[0] = a[0];
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | no sink
        a[0] = 42;
        s[0] = a[0];
        globalThis.ref = a; // escape
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        Array.prototype[2] = 99; // ArrayPrototypeChainIsSaneWatchpoint = IsInvalidated
        let a = new Array(4); // NewArrayWithConstantSize | no sink
        a[0] = 42;
        s[0] = a[2];
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | no sink
        a[0] = 42;
        s[0] = a[0];
        let sliced = a.slice();  // escape
        let mapped = a.map(x => x * 2);  // escape
        return s;
    }
    noInline(test);
    run(test);
}

{
    function test(s) {
        let a = new Array(4); // NewArrayWithConstantSize | no sink
        a[10] = 42;  // Out of bounds
        s[0] = a[0];
        return s;
    }
    noInline(test);
    run(test);
}


