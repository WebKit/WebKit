function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testWithLength(length, index) {
    shouldBe(length >= 1, true);
    var array = [];
    for (var i = 0; i < length; ++i)
        array[i] = i & 0xff;
    array[index] = 0xffef;
    var string = String.fromCharCode.apply(String, array);
    shouldBe(string.length, length);
    for (var i = 0; i < length; ++i) {
        if (index === i)
            shouldBe(string[i], String.fromCharCode(0xffef));
        else
            shouldBe(string[i], String.fromCharCode(i & 0xff));
    }
}

testWithLength(1e4, 1e4 - 1);
testWithLength(1e4, 1e3);
testWithLength(1, 0);
testWithLength(2, 1);
testWithLength(2, 0);
