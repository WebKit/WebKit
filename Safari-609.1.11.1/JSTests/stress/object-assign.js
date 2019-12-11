function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var obj = {
    a: 1,
    b: 2,
    c: 3,
    d: null,
    e: 'e'
};

function test(src) {
    var o = {};
    var keys = Object.keys(src);
    for (var i = 0; i < keys.length; ++i) {
        var key = keys[i];
        o[key] = src[key];
    }
    return o;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    shouldBe(JSON.stringify(test(obj)), `{"a":1,"b":2,"c":3,"d":null,"e":"e"}`);
