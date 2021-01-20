//@ crashOK!
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var putter = function(o) {
    o._unsupported = not_string;
}

var object;
var counter = 0;
var not_string = {
    toString() {
        counter++;
        object.ok = 42;
        return "Hey";
    }
};

object = $vm.createObjectDoingSideEffectPutWithoutCorrectSlotStatus();
object._unsupported = 50;

for (var i = 0; i < 1000; ++i) {
    object = $vm.createObjectDoingSideEffectPutWithoutCorrectSlotStatus();
    putter(object);
    shouldBe(object._unsupported, "Hey");
    // At some point, semantically incorrect result appears. `object.ok = 42` does not invoke ::put.
    // But the structure is still correct.
    if (object.ok !== 42 && object.ok !== "42")
        throw new Error("incorrect");
}
shouldBe(counter, 1000);
