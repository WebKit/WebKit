function test(object) {
    return String(object);
}
noInline(test);

var object = { __proto__: { __proto__: { } } };
for (var i = 0; i < 1e6; ++i) {
    test(object);
}
