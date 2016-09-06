function bar(map) {
    for (let [key, value] of map) {
        if (value - 1 !== key)
            throw new Error("Bad iteration!");
    }
}
noInline(bar);

function foo() {
    let map = new Map;
    for (let i = 0; i < 10000; i++)
        map.set(i, i+1);

    let start = Date.now();
    for (let i = 0; i < 100; i++)
        bar(map);
    const verbose = false;
    if (verbose)
        print(Date.now() - start);

}
foo();
