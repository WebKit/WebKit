//@ runDefault("--thresholdForGlobalLexicalBindingEpoch=2")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

foo1 = 1;
foo2 = 2;
function get1() {
    return foo1;
}
noInline(get1);

function get2() {
    return foo2;
}
noInline(get2);

function get1If(condition) {
    if (condition)
        return foo1;
    return -1;
}
noInline(get1If);

function get2If(condition) {
    if (condition)
        return foo2;
    return -1;
}
noInline(get2If);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(get1(), 1);
    shouldBe(get2(), 2);
    shouldBe(get1(), 1);
    shouldBe(get2(), 2);
    shouldBe(get1If(true), 1);
    shouldBe(get2If(true), 2);
    shouldBe(get1If(false), -1);
    shouldBe(get2If(false), -1);
}

$.evalScript(`const foo1 = 41;`);
$.evalScript(`const foo2 = 42;`);

for (var i = 0; i < 1e3; ++i) {
    shouldBe(get1(), 41);
    shouldBe(get2(), 42);
    shouldBe(get1(), 41);
    shouldBe(get2(), 42);
    shouldBe(get1If(false), -1);
    shouldBe(get2If(false), -1);
}
shouldBe(get1If(true), 41);
shouldBe(get2If(true), 42);
