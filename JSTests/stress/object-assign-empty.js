function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {
    problemId: 0
};

function test(target, source) {
    return Object.assign(target, source);
}
noInline(test);

delete object.problemId;
object.problemId = 2;

for (var i = 0; i < 1e6; ++i) {
    shouldBe(JSON.stringify(test({}, object)), `{"problemId":2}`);
}
