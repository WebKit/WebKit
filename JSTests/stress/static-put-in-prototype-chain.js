function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var target = $vm.createCustomTestGetterSetter();
var testObject = { __proto__: target }
var result = {
    result: undefined
};

testObject.customValue = result;

shouldBe(result.result, target);
