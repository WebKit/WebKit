//@ skip if $secAccel3Removed
//@ runDefault("--jitPolicyScale=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${describe(actual)}`);
}

function shouldBeUndefined(actual) {
    if (typeof actual !== "undefined")
        throw new Error(`bad value: expected undefined but got a ${typeof actual}`);
}

let shadow = {};
shadow.p0 = 1;
shadow.y = 13.37;
Function.prototype.input = shadow;

function setup(a) { return a.input; }
noInline(setup);
let setupObj = Object.create(Function.prototype);
for (let i = 0; i < 5000; i++)
    setup(setupObj);

let subject = "q0002048";
subject.match(/.*/);

RegExp.x = 1;

function f(a, n) {
    let b = a;
    if (n > 0) {
        let r = a.input;
        return r.y;
    }
    b.x; b.x; b.x;
    return undefined;
}
noInline(f);

shouldBeUndefined(f(RegExp, 1));

for (let i = 0; i < 100000; i++)
    f(RegExp, 0);

shouldBeUndefined(f(RegExp, 1));
let result = f(RegExp, 1);
shouldBeUndefined(result);
shouldBe(RegExp.input, subject);

let container = [result];
gc();
shouldBeUndefined(container[0]);
