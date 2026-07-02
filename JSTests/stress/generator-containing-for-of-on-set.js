function *find(nodes, p)
{
    for (let array of nodes) {
        if (!array.find((v) => v === p))
            continue;
        yield array;
    }
}
noInline(find);

let set = new Set();
noInline(set[Symbol.iterator]().next);
for (let i = 0; i < 12; ++i)
    set.add([1,2,4,5,5], {});

for (let i = 0; i < 1e4; ++i) {
    for (let v of find(set, i % 2)) {
        if (i % 2 !== 1)
            throw new Error("no result should have been produced here");
        if (!(v instanceof Array))
            throw new Error("result was not an array");
    }
}
