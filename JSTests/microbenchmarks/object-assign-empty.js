function test(options)
{
    var options = Object.assign({ defaultParam: 32 }, options);
    return options;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    test({});
}
