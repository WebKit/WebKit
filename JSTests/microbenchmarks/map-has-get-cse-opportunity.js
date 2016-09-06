function bar(map, p) {
    if (map.has(p))
        return map.get(p);
}
noInline(bar);

function foo() {
    let map = new Map;
    let items = [
        [10, 50],
        ["450", 78],
        [{}, {}],
        [Symbol(), true],
        [undefined, null],
        [true, null],
        [false, true],
        [45.87, {}]
    ];
    for (let [key, value] of items)
        map.set(key, value);
    let start = Date.now();
    for (let i = 0; i < 5000000; i++)
        bar(map, items[i % items.length][0]);
    const verbose = false;
    if (verbose)
        print(Date.now() - start);
}
foo();
