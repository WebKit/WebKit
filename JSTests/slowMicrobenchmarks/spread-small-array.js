//@ skip if $architecture == "x86"

function foo(arg) {
    return [...arg];
}
noInline(foo);

let arrays = [
    [10, 20, 40],
    [10.5, 20.5, 40.5],
    [20, {}, 8],
];

let start = Date.now();
for (let i = 0; i < 10000000; i++) {
    let array = arrays[i % arrays.length];
    foo(array);
}
const verbose = false;
if (verbose)
    print(Date.now() - start);
