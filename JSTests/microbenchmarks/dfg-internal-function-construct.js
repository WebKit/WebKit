function target(func)
{
    return new func();
}
noInline(target);

for (var i = 0; i < 1e4; ++i)
    target(Array);
