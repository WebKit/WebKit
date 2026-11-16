function *find(nodes, p)
{
    for (let [array] of nodes) {
        if (!array.find((v) => v === p))
            continue;
        yield array;
    }
}
noInline(find);

let map = new Map();
noInline(map[Symbol.iterator]().next);
for (let i = 0; i < 12; ++i)
    map.set([1,2,4,5,5], {});

for (let i = 0; i < 1e4; ++i) {
    for (let v of find(map, i % 2)) { }
}
