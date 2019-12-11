function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(num, string)
{
    var regexp = /hello/g;
    regexp.lastIndex = "Cocoa";
    if (num === 2)
        return regexp.lastIndex;
    var result = string.match(regexp);
    if (num === 1) {
        OSRExit();
        return [result, regexp];
    }
    return regexp.lastIndex;
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var num = i % 3;
    switch (num) {
    case 0:
        shouldBe(test(num, "hellohello"), 0);
        break;
    case 1:
        break;
    case 2:
        shouldBe(test(num, "hellohello"), "Cocoa");
        break;
    }
}

var [result, regexp] = test(1, "hellohello");
shouldBe(regexp instanceof RegExp, true);
shouldBe(regexp.lastIndex, 0);
shouldBe(result.length, 2);
shouldBe(result[0], "hello");
shouldBe(result[1], "hello");
