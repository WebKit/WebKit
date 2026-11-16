function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function*gen() { throw new Error(); }

var flag = true;
var g = gen();
try {
    g.next();
    flag = false;
} catch (error) {
    shouldBe(error.stack.startsWith("gen@"), true);
}
shouldBe(flag, true);
