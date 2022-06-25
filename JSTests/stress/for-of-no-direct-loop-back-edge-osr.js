function osrIfFinalTier(v) {
    if (isFinalTier())
        OSRExit();
    return v !== 0;
}

function test(array) {
    let sum = 0;
    for (let v of array) {
        sum += v;
        if (osrIfFinalTier())
            continue;
        return sum;
    }
}
noInline(test);

let array = [1,2,3,4,5,0]
for (let i = 0; i < 1e5; ++i)
    test(array);
