function hello(object, name)
{
    return object[name];
}
noInline(hello);
for (var i = 0; i < 100; ++i)
    hello([0,1,2,3], 1);
hello([0.1,0.2,0.3,0.4], 1);
hello('string', 1);
hello('string', 1);
hello([true, false, true, false], 1);
hello([true, false, true, false], 1);
