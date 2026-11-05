function sum(map, keys) {
    let result = 0;
    for (let i = 0; i < keys.length; ++i)
        result += map[keys[i]];
    return result;
}
noInline(sum);

let map = Object.create(null);
let keys = [];
for (let i = 0; i < 1000; ++i) {
    map[i] = i;
    keys.push(String(i));
}

for (let i = 0; i < 5e3; ++i) {
    let result = sum(map, keys);
    if (result !== 499500)
        throw new Error("bad result: " + result);
}
