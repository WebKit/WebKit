function isArray(array)
{
    return Array.isArray(array);
}
noInline(isArray);

let values = [
    [ 0, false ],
    [ [], true ],
    [ {}, false ],
    [ "Cappuccino", false ],
    [ Symbol("Cocoa"), false ],
    [ null, false ],
    [ undefined, false ],
]

for (var i = 0; i < 1e4; ++i) {
    for (let pair of values) {
        if (isArray(pair[0]) != pair[1])
            throw new Error(`bad value:${String(pair[1])}`);
    }
}
