function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function escape(node) {
}
noInline(escape);

function test() {
    function test2() {
        var hello = 42;
        function ok() {
            return hello;
        }
        return hello;
    }
    test2();
    var result = test2();
    escape(test2);
    return result;
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test(), 42);
