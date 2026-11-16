function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const isConstructor = $vm.createBuiltin("(function (c) { return @isConstructor(c); })");
var object1 = $vm.SimpleObject();
var object2 = $vm.SimpleObject();
function test(object) {
    object.hey = 42; // Emit CheckStructure.
    return isConstructor(object);
}
noInline(test);

for (let i = 0; i < 1e4; ++i) {
    shouldBe(test(object1), true);
    shouldBe(test(object2), true);
}
