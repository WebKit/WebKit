//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function isSymbol(value)
{
    return typeof value === 'symbol';
}
noInline(isSymbol);

for (var i = 0; i < 1e4; ++i) {
    if (!isSymbol(Symbol('Cocoa')))
        throw new Error("out");
}
