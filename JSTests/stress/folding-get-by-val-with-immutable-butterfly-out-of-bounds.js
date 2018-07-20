function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array0 = [1, 2, 3, 4, 5];
var array1 = [1.2, 2.3, 3.4, 4.5, 5.6];
var array2 = ["Hello", "New", "World", "Cappuccino", "Cocoa"];
var array3 = [null, null, null, null, null];
var array4 = [undefined, undefined, undefined, undefined, undefined];
var array5 = [false, true, false, true, false];

function test0()
{
    return array0[5];
}
noInline(test0);

function test1()
{
    return array1[5];
}
noInline(test1);

function test2()
{
    return array2[5];
}
noInline(test2);

function test3()
{
    return array3[5];
}
noInline(test3);

function test4()
{
    return array4[5];
}
noInline(test4);

function test5()
{
    return array5[5];
}
noInline(test5);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(test0(), undefined);
    shouldBe(test1(), undefined);
    shouldBe(test2(), undefined);
    shouldBe(test3(), undefined);
    shouldBe(test4(), undefined);
    shouldBe(test5(), undefined);
}
// Breaking sane chains.
Array.prototype[5] = 42;
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test0(), 42);
    shouldBe(test1(), 42);
    shouldBe(test2(), 42);
    shouldBe(test3(), 42);
    shouldBe(test4(), 42);
    shouldBe(test5(), 42);
}
