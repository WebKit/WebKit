//@ if isFTLEnabled then runFTLNoCJIT else skip end

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var didFTLCompile = false;
var ftlTrue = $vm.ftlTrue;
function test1(array)
{
    didFTLCompile = ftlTrue();
    return 2 in array;
}
noInline(test1);

var array = [1, 2, 3, 4];
ensureArrayStorage(array);
didFTLCompile = false;
for (var i = 0; i < 1e5; ++i)
    shouldBe(test1(array), true);
shouldBe(didFTLCompile, true);

var array = [1, 2, , 4];
ensureArrayStorage(array);
shouldBe(test1(array), false);

var array = [];
ensureArrayStorage(array);
shouldBe(test1(array), false);

function test2(array)
{
    didFTLCompile = ftlTrue();
    return 13 in array;
}
noInline(test2);

var array1 = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14];
ensureArrayStorage(array1);
var array2 = [1, 2];
ensureArrayStorage(array2);
didFTLCompile = false;
for (var i = 0; i < 1e5; ++i)
    shouldBe(test2(array2), false);
shouldBe(didFTLCompile, true);
shouldBe(test2(array2), false);
shouldBe(test2(array1), true);
