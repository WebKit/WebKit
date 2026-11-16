function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test2(object, string) {
    return object["hello" + string];
}
noInline(test2);

function test3(object, string) {
    return object["hello" + string + "hello"];
}
noInline(test3);

var array = [];
for (var i = 0; i < 100; ++i) {
    var object = {};
    object["hello" + i] = i * 2;
    object["hello" + i + "hello"] = i * 3;
    array.push(object);
}

for (var i = 0; i < 1e4; ++i) {
    for (var j = 0; j < 100; ++j) {
        var key = String(j);
        var object = array[j];
        shouldBe(test2(object, key), j * 2);
        shouldBe(test3(object, key), j * 3);
    }
}
