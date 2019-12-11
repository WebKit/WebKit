function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function test(f, iters = 1000) {
    noInline(f);
    for (let i = 0; i < iters; i++)
        f(i);
}

const globalConst = {};
class GlobalClass { }
let globalLet = {};
let f = new Function("", "return globalConst;");
test(function() {
    assert(f() === globalConst);
});

f = new Function("", "return GlobalClass;");
test(function() {
    let ctor = f();
    assert(ctor === GlobalClass);
    assert((new GlobalClass) instanceof GlobalClass);
});


f = new Function("", "return globalLet;");
test(function() {
    assert(f() === globalLet);
});

f = new Function("prop", "x", "globalLet[prop] = x;");
test(function(i) {
    f(i, i);
    assert(globalLet[i] === i);
});

f = new Function("prop", "x", "globalConst[prop] = x;");
test(function(i) {
    f(i, i);
    assert(globalConst[i] === i);
});

f = new Function("", "globalConst = 25");
test(function() {
    let threw = false;
    try {
        f();
    } catch(e) {
        threw = true;
        assert(e.toString() === "TypeError: Attempted to assign to readonly property.")
    }
    assert(threw);
});

f = new Function("", "globalConst = 25");
test(function() {
    let threw = false;
    try {
        f();
    } catch(e) {
        threw = true;
        assert(e.toString() === "TypeError: Attempted to assign to readonly property.")
    }
    assert(threw);
});

f = new Function("", "constTDZ = 25");
test(function() {
    let threw = false;
    try {
        f();
    } catch(e) {
        threw = true;
        assert(e.toString() === "ReferenceError: Cannot access uninitialized variable.")
    }
    assert(threw);
});

f = new Function("", "constTDZ;");
test(function() {
    let threw = false;
    try {
        f();
    } catch(e) {
        threw = true;
        assert(e.toString() === "ReferenceError: Cannot access uninitialized variable.")
    }
    assert(threw);
});

f = new Function("", "letTDZ;");
test(function() {
    let threw = false;
    try {
        f();
    } catch(e) {
        threw = true;
        assert(e.toString() === "ReferenceError: Cannot access uninitialized variable.")
    }
    assert(threw);
});

f = new Function("", "letTDZ = 20;");
test(function() {
    let threw = false;
    try {
        f();
    } catch(e) {
        threw = true;
        assert(e.toString() === "ReferenceError: Cannot access uninitialized variable.")
    }
    assert(threw);
});

f = new Function("", "ClassTDZ");
test(function() {
    let threw = false;
    try {
        f();
    } catch(e) {
        threw = true;
        assert(e.toString() === "ReferenceError: Cannot access uninitialized variable.")
    }
    assert(threw);
});


const constTDZ = 25;
let letTDZ = 25;
class ClassTDZ { }

