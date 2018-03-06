//@ skip if $memoryLimited

function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}
noInline(assert);

let objs = [];
let symbolPool = [];
const numSymbols = 800;
for (let i = 0; i < numSymbols; ++i)
    symbolPool.push(Symbol());

for (let i = 0; i < 10000; i++) {
    let num = (Math.random() * numSymbols) | 0;
    let o = {};
    for (let i = 0; i < num; ++i) {
        o[symbolPool[i]] = 25; 
    }
    objs.push(o);
}

function foo(o) {
    let props = Object.getOwnPropertySymbols(o);
    for (let i = 0; i < props.length; ++i) {
        let s = props[i];
        assert(o.hasOwnProperty(s));
    }
}
noInline(foo);

let start = Date.now();
for (let o of objs) {
    foo(o);
}
