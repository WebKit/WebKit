var object = {
    get test() { return 42; },
    set test(value) { },
    value: 55
};


for (var i = 0; i < 1e6; ++i) {
    Object.getOwnPropertyDescriptor(object, "test");
    Object.getOwnPropertyDescriptor(object, "value");
}
