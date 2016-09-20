function isSymbol(value)
{
    return typeof value === 'symbol';
}
noInline(isSymbol);

for (var i = 0; i < 1e4; ++i) {
    if (!isSymbol(Symbol('Cocoa')))
        throw new Error("out");
}
