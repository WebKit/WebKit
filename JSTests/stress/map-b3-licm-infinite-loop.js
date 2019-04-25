let count = 0;
function foo() {
    ++count;
    if (count === 1000000)
        throw new Error;
}
noInline(foo);

function test() {
    let map = new Map();

    let count = 0;
    for (let i = 1000000 % 0; ; ) {
        if (!map.has(i)) {
            map.set(i, i);
        }
        foo();
    }

    return map;
}

try {
    test();
} catch {}
