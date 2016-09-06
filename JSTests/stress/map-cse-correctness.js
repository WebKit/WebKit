function assert(b) {
    if (!b)
        throw new Error("Bad result!");
}
noInline(assert);

function testHas(map, key, f) {
    let first = map.has(key);
    f();
    let second = map.has(key);
    return {first, second};
}
noInline(testHas);

function testGet(map, key, f) {
    let first = map.get(key);
    f();
    let second = map.get(key);
    return {first, second};
}
noInline(testGet);

function foo() {
    let map = new Map;
    for (let i = 0; i < 100000; i++) {
        let key = i;
        map.set(key, key);
        let f = () => map.delete(key);
        noInline(f);
        let {first, second} = testHas(map, key, f);
        assert(first);
        assert(!second);
    }
    for (let i = 0; i < 100000; i++) {
        let key = i;
        map.set(key, key);
        let f = () => {};
        noInline(f);
        let {first, second} = testHas(map, key, f);
        assert(first);
        assert(second);
    }


    for (let i = 0; i < 100000; i++) {
        let key = i;
        let value = {};
        map.set(key, value);
        let f = () => map.delete(key);
        noInline(f);
        let {first, second} = testGet(map, key, f);
        assert(first === value);
        assert(second === undefined);
    }
    for (let i = 0; i < 100000; i++) {
        let key = i;
        let value = {};
        map.set(key, value);
        let f = () => {};
        noInline(f);
        let {first, second} = testGet(map, key, f);
        assert(first === value);
        assert(second === value);
    }
}
foo();
