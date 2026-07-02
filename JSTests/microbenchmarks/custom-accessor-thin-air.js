//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function assert(b) {
    if (!b)
        throw new Error;
}

function test5() {
    function get(o) {
        return o.thinAirCustomGetter;
    }
    noInline(get);

    const proto = $vm.createStaticCustomAccessor();
    const o = {__proto__: proto};
    o.testField = 1337;

    for (let i = 0; i < 500000; ++i) {
        assert(get(o) === 1337);
    }
    proto.xyz = 42;
    for (let i = 0; i < 500000; ++i) {
        assert(get(o) === 1337);
    }
}
test5();
