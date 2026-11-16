"use strict"

// Trivial case where everything could be eliminated.
function iterateEmptyArray()
{
    const array = new Array();
    for (let i = 0; i < 100; ++i) {
        if (array[i] !== undefined)
            throw "Unexpected value in empty array at index i = " + i;
    }
}
noInline(iterateEmptyArray);

for (let i = 1e4; i--;) {
    iterateEmptyArray();
}

// Trivial case but the array needs to be checked.
function getArrayOpaque()
{
    return new Array(10);
}
noInline(getArrayOpaque);

function iterateOpaqueEmptyArray()
{
    const array = getArrayOpaque();
    for (let i = 0; i < 100; ++i) {
        if (array[i] !== undefined)
            throw "Unexpected value in empty array at index i = " + i;
    }
}
noInline(iterateEmptyArray);

for (let i = 1e4; i--;) {
    iterateOpaqueEmptyArray();
}