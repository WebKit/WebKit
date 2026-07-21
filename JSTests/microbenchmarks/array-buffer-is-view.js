function isView(value)
{
    return ArrayBuffer.isView(value);
}
noInline(isView);

let buffer = new ArrayBuffer(64);
let values = [
    [ new Int32Array(buffer), true ],
    [ new Uint8Array(4), true ],
    [ new Float64Array(2), true ],
    [ new DataView(buffer), true ],
    [ buffer, false ],
    [ [], false ],
    [ {}, false ],
    [ 42, false ],
    [ "text", false ],
    [ null, false ],
    [ undefined, false ],
];

for (var i = 0; i < 1e6; ++i) {
    for (let pair of values) {
        if (isView(pair[0]) != pair[1])
            throw new Error(`bad value: ${String(pair[0])}`);
    }
}
