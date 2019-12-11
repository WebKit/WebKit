
function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}
noInline(assert);

let objs = [];
let keyPool = [];
let symbolPool = [];
const numKeys = 300;
for (let i = 0; i < numKeys; ++i) {
    keyPool.push(i + "foo");
    symbolPool.push(Symbol("Foo"));
}

for (let i = 0; i < 2000; i++) {
    let num = (Math.random() * numKeys) | 0;
    let o = {};
    for (let i = 0; i < num; ++i) {
        o[keyPool[i]] = 25; 
        o[symbolPool[i]] = 40; 
    }
    objs.push(o);
}

let time = 0;
function foo(o) {
    let props = Object.getOwnPropertyNames(o);
    props.push(...Object.getOwnPropertySymbols(o));
    let start = Date.now();
    for (let i = 0; i < props.length; ++i) {
        let s = props[i];
        assert(o.hasOwnProperty(s));
    }
    time += Date.now() - start;
}
noInline(foo);

for (let o of objs) {
    foo(o);
}
const verbose = false;
if (verbose)
    print(time);
