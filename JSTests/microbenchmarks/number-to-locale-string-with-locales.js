function test(value)
{
    return value.toLocaleString("en-US");
}
noInline(test);

for (var i = 0; i < 5e4; ++i)
    test(i);
