//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
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
