function test()
{
    var object = Object.create(Object.prototype);
    return Object.assign(object, {});
}
noInline(test);
var object = Object.create(Object.prototype);
object.ok = 42;

for (var i = 0; i < 100; ++i) {
    test();
}
