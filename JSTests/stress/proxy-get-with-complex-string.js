function test(object, value) {
    return object[value];
}
noInline(test);

var proxy = new Proxy({}, {});
var object = { hello: 42 };
for (var i = 0; i < 1e6; ++i) {
    test(proxy, "hello");
    test(object, "hello");
}
