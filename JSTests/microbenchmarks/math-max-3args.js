function max3(a, b, c)
{
    return Math.max(a, b, c);
}
noInline(max3);

for (var i = 0; i < 1e6; ++i) {
    max3(5, 5, 3);
    max3(5, 400.2, 1);
    max3(400.2, 5, 2.5);
    max3(24.3, 400.2, 8);
}
