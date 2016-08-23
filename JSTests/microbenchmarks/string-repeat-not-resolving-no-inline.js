function test(str, count)
{
    return str.repeat(count);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test(i.toString(), i);
