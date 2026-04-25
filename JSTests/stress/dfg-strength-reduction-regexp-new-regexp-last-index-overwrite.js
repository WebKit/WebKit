function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}
noInline(shouldBe);

// DFG strength reduction folds RegExpTest/RegExpExec when the RegExp object
// comes from a NewRegExp node and the input string is constant. It scans
// backwards to find the most recent SetRegExpObjectLastIndex in the same block.
// It must not then clobber that discovered value with NewRegExp's initial
// lastIndex (which is always 0).

function testGlobal() {
    var re = /a/g;
    re.lastIndex = 1;
    // "a".length === 1, so searching from index 1 must fail.
    return re.test("a");
}
noInline(testGlobal);

function testSticky() {
    var re = /a/y;
    re.lastIndex = 1;
    // sticky: must match exactly at index 1, which is out of bounds.
    return re.test("a");
}
noInline(testSticky);

function testExecGlobal() {
    var re = /a/g;
    re.lastIndex = 2;
    // "xa".length === 2, searching from index 2 must return null.
    return re.exec("xa");
}
noInline(testExecGlobal);

function testStickyMidString() {
    var re = /a/y;
    re.lastIndex = 1;
    // index 1 is "b", sticky fails there.
    return re.test("ab");
}
noInline(testStickyMidString);

function testGlobalLastIndexUpdated() {
    var re = /a/g;
    re.lastIndex = 1;
    re.test("aa"); // matches at index 1
    // lastIndex should be advanced to 2 (end of match), not 1 (end of match from 0).
    return re.lastIndex;
}
noInline(testGlobalLastIndexUpdated);

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(testGlobal(), false);
    shouldBe(testSticky(), false);
    shouldBe(testExecGlobal(), null);
    shouldBe(testStickyMidString(), false);
    shouldBe(testGlobalLastIndexUpdated(), 2);
}
