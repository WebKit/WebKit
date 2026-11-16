function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

// Strings with the same first character, last character, and length share a cache slot.
function strings(middle) {
    let result = [];
    for (let i = 0; i < 10; ++i) {
        result.push(`a${i}${middle}z`);
        result.push(`あ${i}${middle}ん`);
    }
    return result;
}

for (let middle of ["", "bcdefgh", "bcdefghijklmnopqrstuvw"]) {
    let values = strings(middle);
    for (let iteration = 0; iteration < testLoopCount; ++iteration) {
        let a = values[iteration % values.length];
        let b = values[(iteration * 7 + 3) % values.length];
        if (a === b)
            continue;
        let parsed = JSON.parse(`{"${a}":"${b}","${b}":["${a}","${b}"]}`);
        let keys = Object.keys(parsed);
        shouldBe(keys[0], a);
        shouldBe(parsed[a], b);
        shouldBe(keys[1], b);
        shouldBe(parsed[b][0], a);
        shouldBe(parsed[b][1], b);
    }
}
