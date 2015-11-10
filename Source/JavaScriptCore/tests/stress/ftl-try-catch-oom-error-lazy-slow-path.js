forceGCSlowPaths(); // Force OOM error in FTL MakeRope to happen in a lazy slow path.

function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

function a() { return "a"; }
noInline(a);
function b() { return "b"; }
noInline(b);
function c() { return "c"; }
noInline(c);
function d() { return "d"; }
noInline(d);
function e() { return "e"; }
noInline(e);
function f() { return "f"; }
noInline(f);
function g() { return "g"; }
noInline(g);

let expString = "a";
let exponentialBlowup = false;
let shouldBreak = false;
function foo(fun, left, right) {
    let x = fun();
    let r = left + right;

    var _a = a();
    var _b = b();
    var _c = c();
    var _d = d();
    var _e = e();
    var _f = f();
    var _g = g();
    try {
        expString = expString + expString;
    } catch(e) {
        shouldBreak = true;

        assert(_b === "b");
        assert(_c === "c");
        assert(_d === "d");
        assert(_e === "e");
        assert(_f === "f");
        assert(_g === "g");
    }
    return x + r;
}
noInline(foo);



function blah() { return "blah"; }
noInline(blah);

for (let i = 0; i < 100000; i++) {
    assert(foo(blah, "b", "a") === "blahba");
    if (!exponentialBlowup)
        expString = "a";
}

exponentialBlowup = true;
while (true) {
    assert(foo(blah, "a", "b") === "blahab");
    if (shouldBreak)
        break;
}
