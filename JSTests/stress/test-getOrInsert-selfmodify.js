let map = new Map();
for (let i = 0; i < 100; ++i) {
    map.set("a" + i, 1);
}

map.getOrInsertComputed("b", () => {
    for (let i = 0; i < 100; ++i) {
        map.delete("a" + i);
    }
});

let weakmap = new WeakMap();
let keys = [];
for (let i = 0; i < 100; ++i) {
    keys.push({"key": "a" + i});
    weakmap.set(keys[i], 1);
}

let obj = {"key": "b"}
weakmap.getOrInsertComputed(obj, () => {
    for (let i = 0; i < 100; ++i) {
        weakmap.delete(keys[i]);
    }
});

