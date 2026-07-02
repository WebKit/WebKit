function test(object, string) {
    return object["Visit" + string];
}
noInline(test);

var object = {};
for (let i = 0; i < 1e6; ++i) {
    test(object, "T0");
    test(object, "T1");
    test(object, "T1");
    test(object, "T2");
    test(object, "T3");
    test(object, "T4");
    test(object, "T5");
    test(object, "T6");
    test(object, "T7");
    test(object, "T8");
    test(object, "T9");
}
