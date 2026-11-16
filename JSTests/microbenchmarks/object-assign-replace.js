function test(target, source)
{
    Object.assign(target, source);
}
noInline(test);

var target = {};
var source = {
    a:42, b:42, c:42
};
for (var i = 0; i < 2e6; ++i)
    test(target, source);
