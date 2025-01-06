function max(a, b)
{
    return Math.max(a, b);
}
noInline(max);

for (var i = 0; i < 1e6; ++i) {
    max(5, 5);
    max(5, 400.2);
    max(400.2, 5);
    max(24.3, 400.2);
}
