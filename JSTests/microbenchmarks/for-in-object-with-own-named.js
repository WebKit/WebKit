function sum(o) {
    let sum = 0;
    for (let p in o)
        sum += o[p];
    return sum;
}

function opaqueSet(o) {
    o.z = 3;
}

let object = { x: 1, y: 2 };
opaqueSet(object);
for (let i = 0; i < 1e5; ++i) {
    if (sum(object) !== 6)
        throw new Error("bad sum");
}
