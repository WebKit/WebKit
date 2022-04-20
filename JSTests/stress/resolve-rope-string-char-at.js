function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var hello0 = "Hello"; // Ensure that "Hello" is registered in AtomStringTable.
var hello1 = createNonRopeNonAtomString("Hello");

function test(string)
{
    var result = ["", "", "", ""];
    var object = { };
    for (var i = 0; i < 10000; ++i) {
        var index = i % 4;
        result[index] = string.charAt(index);
        if (i === 5000) {
            // Enforce JSValue::toPropertyKey. After this, string is atomic.
            object[string];
        }
    }
    return result;
}
noInline(test);

for (var i = 0; i < 1000; ++i) {
    var newString = createNonRopeNonAtomString("Hello");
    var result = test(newString)
    shouldBe(result[0], 'H');
    shouldBe(result[1], 'e');
    shouldBe(result[2], 'l');
    shouldBe(result[3], 'l');
}
