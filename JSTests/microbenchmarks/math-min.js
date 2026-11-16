function min(a, b)
{
    return Math.min(a, b);
}
noInline(min);

for (var i = 0; i < 1e6; ++i) {
    min(5, 5);
    min(5, 400.2);
    min(400.2, 5);
    min(24.3, 400.2);
}
