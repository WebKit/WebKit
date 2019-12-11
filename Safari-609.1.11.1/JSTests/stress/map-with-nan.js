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
    var map = new Map();
    map.set(NaN1(), 1);
    map.set(NaN2(), 2);
    map.set(NaN3(), 3);
    map.set(NaN4(), 4);
    return map.size;
}
noInline(test1);

function test2()
{
    return new Map([
        [NaN1(), 1],
        [NaN2(), 2],
        [NaN3(), 3],
        [NaN4(), 4]
    ]).size;
}
noInline(test2);

function test3()
{
    var map = new Map();
    map.set(NaN1NoInline(), 1);
    map.set(NaN2NoInline(), 2);
    map.set(NaN3NoInline(), 3);
    map.set(NaN4NoInline(), 4);
    return map.size;
}
noInline(test3);

function test4()
{
    return new Map([
        [NaN1NoInline(), 1],
        [NaN2NoInline(), 2],
        [NaN3NoInline(), 3],
        [NaN4NoInline(), 4]
    ]).size;
}
noInline(test4);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(test1(), 1);
    shouldBe(test2(), 1);
    shouldBe(test3(), 1);
    shouldBe(test4(), 1);
}
