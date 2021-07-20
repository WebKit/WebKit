function test(target, source)
{
    Object.assign(target, source);
}
noInline(test);

test({}, undefined);
test({}, null);
for (var i = 0; i < 1e4; ++i)
    test({}, {});
test({}, undefined);
test({}, null);
for (var i = 0; i < 1e4; ++i)
    test({}, undefined);
