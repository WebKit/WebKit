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

    for (let i = 0; i < 500; ++i) {
        assert(get(o) === 1337);
    }
    proto.xyz = 42;
    for (let i = 0; i < 500; ++i) {
        assert(get(o) === 1337);
    }
}
test5();
