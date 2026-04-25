"use strict"

function opaqueGetByValKnownArray(value)
{
    let array = [];
    return array[value];
}
noInline(opaqueGetByValKnownArray);

// Warm up without out-of-bounds access.
for (let i = 0; i < 1e3; ++i) {
    if (opaqueGetByValKnownArray(0) !== undefined)
        throw "Failed opaqueGetByValKnownArray(0)";
}

// Then access out of bounds.
for (let i = 0; i < 1e3; ++i) {
    if (opaqueGetByValKnownArray(-1) !== undefined)
        throw "Failed opaqueGetByValKnownArray(-1)";
}