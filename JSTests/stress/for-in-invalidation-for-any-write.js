function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}
noInline(assert);

function test(f) {
    noInline(f);
    for (let i = 0; i < 1000; ++i)
        f();
}

test(function() {
    let o = {xx: 42};
    for (let i in o) {
        for (let j = 0; j < 2; j++) {
            let r = o[i];
            if (i === "xx")
                assert(r === 42);
            i = function() { }
        }
    }
});

test(function() {
    let o = {xx: 42};
    for (let i in {xx: 0}) {
        for (let j = 0; j < 2; j++) {
            let r = o[i];
            if (i === "xx")
                assert(r === 42);
            i = new Uint32Array([0, 1, 0x777777, 0, 0]);
        }
    }
});

test(function() {
    let o = {xx: 42};
    for (let i in {xx: 0}) {
        for (let j = 0; j < 2; j++) {
            let r = o[i];
            if (i === "xx")
                assert(r === 42);
            ([i] = [new Uint32Array([0, 1, 0x777777, 0, 0])]);
        }
    }
});

test(function() {
    let o = {xx: 42};
    for (let i in {xx: 0}) {
        for (let j = 0; j < 2; j++) {
            let r = o[i];
            if (i === "xx")
                assert(r === 42);
            ({xyz: i} = {xyz: new Uint32Array([0, 1, 0x777777, 0, 0])});
        }
    }
});

test(function() {
    let o = [1,2,3];
    let toStringCalls = 0;
    let first;
    let num = 0;
    let total = 0;
    for (let i in o) {
        first = true;
        for (let j = 0; j < 3; j++) {
            let r = o[i];
            if (first)
                assert(r === o[num]);
            else
                assert(r === undefined);
            first = false;
            i = {
                toString() {
                    ++toStringCalls;
                    return "hello!";
                }
            }
        }
        ++num;
    }

    // Should be called twice per outer for-in loop.
    assert(toStringCalls === o.length * 2);
});

test(function() {
    let o = [1,2,3];
    let toStringCalls = 0;
    let first;
    let num = 0;
    let total = 0;
    for (let i in o) {
        first = true;
        for (let j = 0; j < 3; j++) {
            let r = o[i];
            if (first)
                assert(r === o[num]);
            else
                assert(r === undefined);
            first = false;
            ([i] = [{
                toString() {
                    ++toStringCalls;
                    return "hello!";
                }
            }]);
        }
        ++num;
    }

    // Should be called twice per outer for-in loop.
    assert(toStringCalls === o.length * 2);
});

test(function() {
    let o = [1,2,3];
    let toStringCalls = 0;
    let first;
    let num = 0;
    let total = 0;
    for (let i in o) {
        first = true;
        for (let j = 0; j < 3; j++) {
            let r = o[i];
            if (first)
                assert(r === o[num]);
            else
                assert(r === undefined);
            first = false;
            ({xyz: i} = {xyz: {
                toString() {
                    ++toStringCalls;
                    return "hello!";
                }
            }});
        }
        ++num;
    }

    // Should be called twice per outer for-in loop.
    assert(toStringCalls === o.length * 2);
});
