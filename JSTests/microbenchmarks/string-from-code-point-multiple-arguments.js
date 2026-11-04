function test(a, b, c)
{
    return String.fromCodePoint(a, b, c);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(97 + (i & 15), 98, 99);
