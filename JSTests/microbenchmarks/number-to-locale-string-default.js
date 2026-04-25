function test(value)
{
    return value.toLocaleString();
}
noInline(test);

for (var i = 0; i < 5e4; ++i)
    test(i);
