function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

var array1 = [0, 1, 2, 3, 4, 5];
var array2 = ["Hello", "World", "Cocoa"];
Object.freeze(array1);
Object.freeze(array2);

function test1()
{
    return array1[0] + array1[1] + array1[2] + array1[3] + array1[4] + array1[5];
}
noInline(test1);

function test2()
{
    return array1[0] + array1[1] + array1[2] + array1[3] + array1[4] + array1[5] + (array1[6] | 0);
}
noInline(test2);

function test3()
{
    return array2[0] + array2[1] + array2[2];
}
noInline(test3);

var array3 = [];
Object.defineProperty(array3, 0, {
    get() { return 42; }
});
Object.defineProperty(array3, 1, {
    get() { return 42; }
});
Object.freeze(array3);

function test4()
{
    return array3[0] + array3[1];
}
noInline(test4);

var array4 = [0, 1, 2, 3, 4, 5];
Object.seal(array4);

function test5()
{
    return array4[0] + array4[1] + array4[2] + array4[3] + array4[4] + array4[5];
}
noInline(test5);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(test1(), 15);
    shouldBe(test2(), 15);
    shouldBe(test3(), `HelloWorldCocoa`);
    shouldBe(test4(), 84);
    shouldBe(test5(), 15);
}
