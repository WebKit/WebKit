//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function assert(b) {
    if (!b)
        throw new Error;
}

const Value = $vm.createCustomTestGetterSetter();
function test1() {
    function getCustomValue2(o) {
        return o.customValue2;
    }
    noInline(getCustomValue2);

    Value.foo = 42;

    const o = {};
    o.__proto__ = Value;
    Value.customValue2 = false;

    for (let i = 0; i < 5000000; ++i) {
        assert(getCustomValue2(o) === false);
    }
    delete Value.foo;
    for (let i = 0; i < 5000000; ++i) {
        assert(getCustomValue2(o) === false);
    }
}
test1();
