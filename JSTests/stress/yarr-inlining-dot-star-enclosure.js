function test(string)
{
    return /.*\:.*/.test(string);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    test(String(i));
}
