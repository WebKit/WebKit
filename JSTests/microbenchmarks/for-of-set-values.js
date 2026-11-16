const set = new Set();
for (let i = 0; i < 100; ++i)
    set.add(i);

function test(set) {
    let sum = 0;
    for (let x of set)
        sum += x;
    return sum;
}
noInline(test);

for (let i = 0; i < 1e4; ++i)
    test(set);
