function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function t(array) {
    var result = 0;
    var item = 0;
    for (var i = array.length - 1; item = array[i]; --i) {
        result += item;
    }
    return result;
}

var array = [];
for (var i = 0; i < 10; ++i)
    array.push(i + 1);

for (var i = 0; i < 1e4; ++i)
    shouldBe(t(array), 55);
Array.prototype[-1] = 42;
shouldBe(t(array), 55 + 42);
