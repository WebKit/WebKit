function test(str, count)
{
    var repeated = str.repeat(count);
    // Expand the rope.
    return repeated[0] + repeated[count >> 1] + repeated[repeated.length - 1];
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test(i.toString(), i);
