function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var target = $vm.createCustomTestGetterSetter();
var testObject = { __proto__: target }
var result = {
    result: undefined
};

// This does not invoke customValue setter since it is value.
testObject.customValue = result;

shouldBe(result.result, undefined);

// This invokes customAccessor setter since it is an accessor.
testObject.customAccessor = result;

shouldBe(result.result, testObject);
