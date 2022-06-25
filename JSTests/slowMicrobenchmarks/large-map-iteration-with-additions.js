//@ skip if $architecture == "x86"

let counter = 0;
function bar(map) {
    for (let [key, value] of map) {
        if (Math.random() > 0.95) {
            map.set("" + counter, counter);
            ++counter;
        }
    }
}
noInline(bar);

function foo() {
    let map = new Map;
    for (let i = 0; i < 1000; i++)
        map.set(i, i+1);

    let start = Date.now();
    for (let i = 0; i < 100; i++)
        bar(map);
    const verbose = false;
    if (verbose)
        print(Date.now() - start);

}
foo();
