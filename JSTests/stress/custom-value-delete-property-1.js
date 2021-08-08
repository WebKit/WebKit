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

    const o = {};
    o.__proto__ = Value;
    Value.customValue2 = false;

    for (let i = 0; i < 500; ++i) {
        assert(getCustomValue2(o) === false);
    }
    delete Value.customValue2;
    assert(getCustomValue2(o) === undefined);
}
test1();
