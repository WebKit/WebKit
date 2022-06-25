function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function div(a, b) {
    return a / b;
}
noInline(div);

function NaN1() {
    return div(0, 0);
}
function NaN2() {
    return NaN;
}
function NaN3() {
    return Infinity/Infinity;
}
function NaN4() {
    return NaN + NaN;
}

function NaN1NoInline() {
    return div(0, 0);
}
noInline(NaN1NoInline);
function NaN2NoInline() {
    return NaN;
}
noInline(NaN2NoInline);
function NaN3NoInline() {
    return Infinity/Infinity;
}
noInline(NaN3NoInline);
function NaN4NoInline() {
    return NaN + NaN;
}
noInline(NaN4NoInline);

function test1()
{
    var set = new Set();
    set.add(NaN1());
    set.add(NaN2());
    set.add(NaN3());
    set.add(NaN4());
    return set.size;
}
noInline(test1);

function test2()
{
    return new Set([NaN1(), NaN2(), NaN3(), NaN4()]).size;
}
noInline(test2);

function test3()
{
    var set = new Set();
    set.add(NaN1NoInline());
    set.add(NaN2NoInline());
    set.add(NaN3NoInline());
    set.add(NaN4NoInline());
    return set.size;
}
noInline(test3);

function test4()
{
    return new Set([NaN1NoInline(), NaN2NoInline(), NaN3NoInline(), NaN4NoInline()]).size;
}
noInline(test4);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(test1(), 1);
    shouldBe(test2(), 1);
    shouldBe(test3(), 1);
    shouldBe(test4(), 1);
}
