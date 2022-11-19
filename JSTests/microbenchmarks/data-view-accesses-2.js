//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ $skipModes << :lockdown if $buildType == "debug"
"use strict";

function assert(b, m = "") {
    if (!b)
        throw new Error("Bad: " + m);
}

let getOps = {
    getUint8: 1,
    getUint16: 2,
    getUint32: 4,
    getInt8: 1,
    getInt16: 2,
    getInt32: 4,
    getFloat32: 4,
    getFloat64: 8,
};

let setOps = {
    setUint8: 1,
    setUint16: 2,
    setUint32: 4,
    setInt8: 1,
    setInt16: 2,
    setInt32: 4,
    setFloat32: 4,
    setFloat64: 8,
};

let getFuncs = [];
for (let p of Object.keys(getOps)) {
    let byteSize = getOps[p];
    let str = `
        (function (dv, byteLength) {
            let sum = 0;
            for (let i = 0; i < byteLength; i += ${byteSize}) {
                sum += dv.${p}(i, false);
            }
            return sum;
        });
        `;
       
    let func = eval(str);
    noInline(func);
    getFuncs.push(func);
}

let setFuncs = [];
for (let p of Object.keys(setOps)) {
    let byteSize = setOps[p];
    let value = 10;
    if (p.indexOf("float") !== -1)
        value = 10.5;
    let str = `
        (function (dv, byteLength) {
            for (let i = 0; i < byteLength; i += ${byteSize}) {
                dv.${p}(i, ${value}, false);
            }
        });
        `;
       
    let func = eval(str);
    noInline(func);
    setFuncs.push(func);
}

function test() {
    const size = 16*1024;
    let ab = new ArrayBuffer(size);
    let dv = new DataView(ab);
    for (let i = 0; i < 1000; ++i) {
        for (let f of getFuncs)
            f(dv, size);
        for (let f of setFuncs)
            f(dv, size);
    }
}
test();
