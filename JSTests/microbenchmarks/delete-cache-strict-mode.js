"use strict";

let objs = [
    {x: 42},
    {y: 42, z: 50},
    {a: 42, b: 55}
];

let idents = [
    "doesNotExist",
    "doesNotExist2"
];

function assert(b) {
    if (!b)
        throw new Error;
}

function doDel(o) {
    return delete o.doesNotExist;
}
noInline(doDel);

function doDelByVal(o, v) {
    return delete o[v];
}
noInline(doDelByVal);

for (let i = 0; i < 1000000; ++i) {
    for (let i = 0; i < objs.length; ++i) {
        assert(doDel(objs[i]));

        for (let j = 0; j < idents.length; ++j)
            assert(doDelByVal(objs[i], idents[j]));
    }
}
