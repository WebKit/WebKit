function assert(b) {
    if (!b)
        throw new Error("Bad!");
}
noInline(assert);

let map = new Map;

function intKey(key) {
    key++;
    key--;
    return map.get(key);
}
noInline(intKey);

function doubleKey(key) {
    key += 0.5;
    return map.get(key);
}
noInline(doubleKey);

function objectKey(o) {
    o.f;
    return map.get(o);
}
noInline(objectKey);

function stringKey(s) {
    s = s + s;
    s = s.toString();
    return map.get(s);
}
noInline(stringKey);

function booleanKey(boolValue) {
    return map.get(!boolValue);
}
noInline(booleanKey);

function symbolKey(o) {
    let symbol = o.symbol;
    return map.get(symbol);
}
noInline(symbolKey);

function cellKey(k) {
    return map.get(k);
}
noInline(cellKey);

const iters = 300000;
let start = Date.now();

map.set(20, 50);
for (let i = 0; i < iters; i++) {
    assert(intKey(20) === 50);
}

map.set(20.5, 55);
for (let i = 0; i < iters; i++) {
    assert(doubleKey(20.0) === 55);
}

map.set(30.0, 60);
for (let i = 0; i < iters; i++) {
    assert(doubleKey(29.5) === 60);
}

{
    let o = {f: 20};
    map.set(o, o);
    for (let i = 0; i < iters; i++) {
        assert(objectKey(o) === o);
    }
    let str = "93421njkfsadhjklfsdahuy9i4312";
    map.set(str, str);
    for (let i = 0; i < 100; i++)
        assert(objectKey(str) === str);
}

{
    let s = "foofoo";
    map.set(s, s);
    for (let i = 0; i < iters; i++) {
        assert(stringKey("foo") === s);
    }
}

map.set(true, "abc");
for (let i = 0; i < iters; i++) {
    assert(booleanKey(false) === "abc");
}

map.set(false, "abcd");
for (let i = 0; i < iters; i++) {
    assert(booleanKey(true) === "abcd");
}

{
    let symbol = Symbol();
    let o = {symbol};
    map.set(symbol, o);
    for (let i = 0; i < iters; i++) {
        assert(symbolKey(o) === o);
    }
}

{
    let o1 = {};
    let o2 = {};
    let s1 = "s1s1s1s1s1s1";
    let s2 = "s1s1s1s1s1s1s2";
    map.set(o1, o2);
    map.set(s1, s2);
    let keys = [o1, o2, s1, s2];
    for (let i = 0; i < iters; i++) {
        let k = keys[i % keys.length];
        let result = k === o1 ? o2 : 
                     k === s1 ? s2 :
                     undefined;
        assert(cellKey(k) === result);
    }
}

const verbose = false;
if (verbose)
    print(Date.now() - start);
