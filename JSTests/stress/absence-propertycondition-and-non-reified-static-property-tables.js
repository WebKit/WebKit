// Absence PropertyCondition should consult non-reified static property tables;
// otherwise, the Abstract Interpreter folds GetById past Symbol.prototype's
// static `description`.

function assert(cond) {
    if (!cond)
        throw new Error("Assertion failed");
}

function assertThrows(fn) {
    try {
        fn();
    } catch (e) {
        return;
    }

    throw "Expected an exception";
}

let K = {};
K.y = 13.37;
Object.prototype.description = K;

function setup(a) { return a.description; }
noInline(setup);
let setupObj = {};
for (let i = 0; i < 50_000; i++) setup(setupObj);

let o_warm  = Object(Symbol("warm")); o_warm.x  = 1;
let o_leak  = Object(Symbol("leak")); o_leak.x  = 1;
let o_crash = Object(Symbol());       o_crash.x = 1;

function f(a, n) {
    let b = a;
    if (n > 0) {
        let r = a.description;
        let v = r.y;
        return v;
    }
    b.x; b.x; b.x;
    return undefined;
}
noInline(f);

assert(f(o_warm, 1) === undefined);
for (let i = 0; i < 500_000; i++) f(o_warm, 0);

var hole = f(o_leak, 1);
assert(hole === undefined);
assert(typeof hole === "undefined");

assertThrows(() => f(o_crash, 1));
