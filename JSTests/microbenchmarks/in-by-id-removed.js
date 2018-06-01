function test(object)
{
    if ("cocoa" in object)
        return object.cocoa;
    return 0;
}
noInline(test);

var o1 = {
    cocoa: 42
};
var o2 = {
    cocoa: 40,
    cappuccino: 41
};

for (var i = 0; i < 1e6; ++i) {
    test(o1);
    test(o2);
}
