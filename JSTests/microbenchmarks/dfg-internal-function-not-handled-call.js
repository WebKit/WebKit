function target(func)
{
    return func();
}
noInline(target);

for (var i = 0; i < 1e4; ++i)
    target(Date);
