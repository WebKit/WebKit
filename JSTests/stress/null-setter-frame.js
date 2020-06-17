function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var realm = runString(`
function test(object)
{
    "use strict";
    object.value = 42;
}
noInline(test);
test
`);

var object = {
    get value() { return 42; }
};
for (var i = 0; i < 1e4; ++i) {
    var thrown = false;
    try {
        realm.test(object);
    } catch (error) {
        thrown = true;
        shouldBe(error.constructor, realm.TypeError);
    }
    shouldBe(thrown, true);
}
