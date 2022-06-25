//@ skip if $memoryLimited

function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}
noInline(assert);

let objs = [];
let keyPool = [];
const numKeys = 800;
for (let i = 0; i < numKeys; ++i)
    keyPool.push(i + "foo");

for (let i = 0; i < 10000; i++) {
    let num = (Math.random() * numKeys) | 0;
    let o = {};
    for (let i = 0; i < num; ++i) {
        o[keyPool[i]] = 25; 
    }
    objs.push(o);
}

function foo(o) {
    let props = Object.getOwnPropertyNames(o);
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
const verbose = false;
if (verbose)
    print(Date.now() - start);
