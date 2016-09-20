function isSymbol(value)
{
    return typeof value === 'symbol';
}
noInline(isSymbol);

var list = [
    [ {}, false ],
    [ [] , false ],
    [ "Cappuccino", false ],
    [ Symbol('Cocoa'), true ],
    [ null, false ],
    [ undefined, false ],
    [ 42, false ],
]

for (var i = 0; i < 1e4; ++i) {
    for (let pair of list) {
        if (isSymbol(pair[0]) != pair[1])
            throw new Error(`bad value:${String(pair[0])}, ${pair[1]}`);
    }
}
