function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [1, 2, 3, 4, 5];
var array2 = [1, "HELLO", 3, 4, 5];
var array3 = [0.1, "OK", 0.3, 0.4, 0.5];
ensureArrayStorage(array2);
array.ok = 42;
array2.ok = 42;
array3.ok = 42;

// Arrayify(ArrayStorage) works with ftl-eager
function testArrayStorage(array)
{
    return array.length;
}
noInline(testArrayStorage);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(testArrayStorage(array), 5);
    shouldBe(testArrayStorage(array2), 5);
    shouldBe(testArrayStorage(array3), 5);
}

var array4 = {0:1, 1:"HELLO", 2:3, 3:4, 4:5};
ensureArrayStorage(array4);
shouldBe(testArrayStorage(array4), undefined);
