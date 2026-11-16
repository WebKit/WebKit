function test(a, b) {
    return Object.is(a, b);
}
noInline(test);

var object0 = { };
var object1 = { };
var object2 = { };
var object3 = { };

for (var i = 0; i < 1e6; ++i) {
    test(object0, object1);
    test(object1, object2);
    test(object2, object3);
    test(object3, object0);
}
