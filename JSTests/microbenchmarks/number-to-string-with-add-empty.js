function toStringLeft(num)
{
    return num + '';
}
noInline(toStringLeft);

function toStringRight(num)
{
    return '' + num;
}
noInline(toStringRight);

for (var i = 0; i < 1e5; ++i) {
    toStringLeft(i);
    toStringRight(i);
}
