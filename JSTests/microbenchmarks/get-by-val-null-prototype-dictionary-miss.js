function get(map, key) {
    return map[key];
}
noInline(get);

function countHits(map, keys) {
    let count = 0;
    for (let i = 0; i < keys.length; ++i) {
        if (map[keys[i]] !== undefined)
            count++;
    }
    return count;
}
noInline(countHits);

let map = Object.create(null);
for (let i = 0; i < 1000; ++i)
    map["key" + i] = i;
for (let i = 0; i < 1e4; ++i)
    get(map, "missing");
map.added = 0;
delete map.key0;

let keys = [];
for (let i = 0; i < 1000; ++i)
    keys.push("missing" + i);

let count = 0;
for (let i = 0; i < 1e4; ++i)
    count += countHits(map, keys);
if (count)
    throw new Error("bad count: " + count);
