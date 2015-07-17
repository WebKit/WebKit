description('Test Promise nested microtasks.');

var globalObject = this;
globalObject.jsTestIsAsync = true;

var value1;
var value2;
var result;
Promise.resolve(42).then(function (v1) {
    value1 = v1;
    shouldBe('value1', '42');
    shouldBeUndefined('value2');
    shouldBeUndefined('result');
    return Promise.resolve(84).then(function (v2) {
        value2 = v2;
        shouldBe('value2', '84');
        shouldBeUndefined('result');
        return v2 * v1;
    });
}).then(function (r) {
    result = r;
    shouldBe('result', '3528');
    finishJSTest();
});

debug('The promise is not fulfilled until after this function call executes.');
