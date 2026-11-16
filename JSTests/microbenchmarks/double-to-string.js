function test(num) {
    return num + 'px';
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(i + 30.4);
