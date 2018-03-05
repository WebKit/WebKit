function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testArrayStorageInBounds(array, index, value)
{
    array[index] = value;
}
noInline(testArrayStorageInBounds);

for (var i = 0; i < 1e4; ++i) {
    var array = [1, 2, 3, 4, 5];
    var object = { a: 10 };
    Object.defineProperties(object, {
        "1": {
            get: function() { return this.a; },
            set: function(x) { this.a = x; },
        },
    });
    array.__proto__ = object;
    ensureArrayStorage(array);
    shouldBe(array[0], 1);
    testArrayStorageInBounds(array, 0, 42);
    shouldBe(array[0], 42);
}
for (var i = 0; i < 1e4; ++i) {
    var array = [, 2, 3, 4];
    var object = { a: 10 };
    Object.defineProperties(object, {
        "1": {
            get: function() { return this.a; },
            set: function(x) { this.a = x + 20; },
        },
    });
    array.__proto__ = object;
    ensureArrayStorage(array);
    shouldBe(array[0], undefined);
    shouldBe(array[1], 2);
    testArrayStorageInBounds(array, 0, 42);
    testArrayStorageInBounds(array, 1, 40);
    shouldBe(array[0], 42);
    shouldBe(array[1], 40);
    testArrayStorageInBounds(array, 4, 42);
    shouldBe(array[4], 42);
    shouldBe(array.length, 5);
}
for (var i = 0; i < 1e4; ++i) {
    var array = [, , 3, 4];
    var object = { a: 10 };
    Object.defineProperties(object, {
        "1": {
            get: function() { return this.a; },
            set: function(x) { this.a = x + 20; },
        },
    });
    array.__proto__ = object;
    ensureArrayStorage(array);
    shouldBe(array[0], undefined);
    shouldBe(array[1], 10);
    shouldBe(array[6], undefined);
    testArrayStorageInBounds(array, 6, 42);
    testArrayStorageInBounds(array, 1, 42);
    shouldBe(array.length, 7);
    shouldBe(array[1], 62);
    shouldBe(array[6], 42);
}
