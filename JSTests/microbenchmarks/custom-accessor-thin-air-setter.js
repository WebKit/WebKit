//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function assert(b) {
    if (!b)
        throw new Error;
}

function test5() {
    function set(o, value) {
        o.customAccessor = value;
    }
    noInline(set);

    const o = $vm.createCustomTestGetterSetter();

    for (let i = 0; i < 500000; ++i) {
        set(o, 1337);
    }
    for (let i = 0; i < 500000; ++i) {
        set(o, 1337);
    }
}
test5();
