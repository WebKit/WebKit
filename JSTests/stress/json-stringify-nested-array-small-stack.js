//@ runDefault("--maxPerThreadStackUsage=524288")

for (let depth of [10, 1000, 2000, 4000]) {
    let array = [];
    for (let i = 0; i < depth; ++i)
        array = [array];
    let result = JSON.stringify(array);
    if (result !== '['.repeat(depth + 1) + ']'.repeat(depth + 1))
        throw new Error('depth ' + depth + ' produced the wrong result');
}
