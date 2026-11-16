function f(x, y, z)
{
    return x + y + z;
}

for (var i = 0; i < 2500000; ++i)
    f(1, 2, 3);
