function shouldBe(a, e, m) { if (a !== e) throw new Error(m + ": got " + a + ", expected " + e); }

function testExec() {
    let r = /a/g;
    let called = 0;
    r.lastIndex = { valueOf() { called++; r.compile("b", "g"); return 0; } };
    let m = r.exec("b");
    shouldBe(called, 1, "exec valueOf called once");
    shouldBe(m && m[0], "b", "exec uses recompiled pattern");
    shouldBe(r.lastIndex, 1, "exec lastIndex updated");
}

function testTest() {
    let r = /a/g;
    r.lastIndex = { valueOf() { r.compile("b", "g"); return 0; } };
    shouldBe(r.test("b"), true, "test uses recompiled pattern");
}

function testFlagsChange() {
    let r = /x/;
    r.lastIndex = { valueOf() { r.compile("x", "g"); return 2; } };
    let m = r.exec("__x");
    shouldBe(m && m[0], "x", "exec matches");
    shouldBe(r.lastIndex, 3, "non-global -> global: lastIndex updated");
}

function testFlagsChangeReverse() {
    let r = /x/g;
    r.lastIndex = { valueOf() { r.compile("x", ""); return 5; } };
    let m = r.exec("x____");
    shouldBe(m && m.index, 0, "global -> non-global: matches from 0");
}

function testNonGlobal() {
    let r = /a/;
    r.lastIndex = { valueOf() { r.compile("b"); return 0; } };
    shouldBe(r.exec("b")[0], "b", "non-global exec uses recompiled");
}

for (let i = 0; i < testLoopCount; ++i) {
    testExec();
    testTest();
    testFlagsChange();
    testFlagsChangeReverse();
    testNonGlobal();
}
