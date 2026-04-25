function shouldBe(a, e, m) { if (a !== e) throw new Error(m + ": " + a + " !== " + e); }
function shouldThrow(f, m) { let threw = false; try { f(); } catch (e) { threw = e instanceof TypeError; } if (!threw) throw new Error(m + ": did not throw TypeError"); }

function testConstAnd() {
    const a = true;
    shouldThrow(() => { a &&= 99; }, "const &&=");
    shouldBe(a, true, "const a unchanged after &&=");
    const b = false;
    b &&= (() => { throw new Error("should not evaluate"); })();
    shouldBe(b, false, "const b short-circuit &&=");
}

function testConstOr() {
    const a = false;
    shouldThrow(() => { a ||= 99; }, "const ||=");
    shouldBe(a, false, "const a unchanged after ||=");
    const b = true;
    b ||= (() => { throw new Error("should not evaluate"); })();
    shouldBe(b, true, "const b short-circuit ||=");
}

function testConstCoalesce() {
    const a = null;
    shouldThrow(() => { a ??= 99; }, "const ??=");
    shouldBe(a, null, "const a unchanged after ??=");
    const b = 1;
    b ??= (() => { throw new Error("should not evaluate"); })();
    shouldBe(b, 1, "const b short-circuit ??=");
}

let f = function fn() {
    let r = (fn &&= 42);
    shouldBe(fn, f, "sloppy fn name binding unchanged after &&=");
    shouldBe(r, 42, "expression value is rhs");
};

let g = function fn() {
    "use strict";
    shouldThrow(() => { fn &&= 42; }, "strict fn name binding &&=");
    shouldBe(fn, g, "strict fn name binding unchanged after &&=");
};

for (let i = 0; i < testLoopCount; ++i) {
    testConstAnd();
    testConstOr();
    testConstCoalesce();
    f();
    g();
}
