function test(value)
{
    if (value.flag === true)
        return true;
    return false;
}
noInline(test);

var object = { flag: true };
for (var i = 0; i < 1e6; ++i) {
    test(object);
}
var object = { flag: { } };
for (var i = 0; i < 1e6; ++i) {
    test(object);
}
