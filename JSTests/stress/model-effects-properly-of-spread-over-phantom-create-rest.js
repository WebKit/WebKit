"use strict";
function f1(o) {
    let result = [];
    for (let key of Object.getOwnPropertyNames(o)) {
        result.push(key)
    }
    return result;
}
function f2(a1, a2, ...args) {
    let r = f1(a1);
    let index = r[a2 % r.length];
    a1[index](...args)
}
let theObj = {};
let o2 = {
    valueOf: function (a, b) {
        a === 42
        b === theObj
        try {} catch (e) {}
    }
};
for (let i = 0; i < 1e5; ++i) {
    for (let j = 0; j < 100; j++) {}
    f2(o2, 897989, 42, theObj);
}
