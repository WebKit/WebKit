function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

// Objects whose first keys differ give the empty object's Structure many transitions. Keys sharing their
// first character, last character, and length must still be told apart.
let keys = [];
for (let i = 0; i < 26; ++i) {
    let middle = String.fromCharCode(97 + i);
    keys.push(`a${middle}z`, `a${middle}${middle}z`, `b${middle}`);
}

let parts = [];
for (let i = 0; i < 2000; ++i) {
    let first = keys[i % keys.length];
    let second = keys[(i * 7 + 3) % keys.length];
    if (first === second)
        second = second + "_";
    parts.push(`{"${first}":${i},"${second}":"${i}","${first}${second}":[${i}]}`);
}
let source = `[${parts.join(",")}]`;

for (let iteration = 0; iteration < 3; ++iteration) {
    let result = JSON.parse(source);
    shouldBe(result.length, 2000);
    for (let i = 0; i < result.length; ++i) {
        let first = keys[i % keys.length];
        let second = keys[(i * 7 + 3) % keys.length];
        if (first === second)
            second = second + "_";
        let object = result[i];
        shouldBe(JSON.stringify(Object.keys(object)), JSON.stringify([first, second, first + second]));
        shouldBe(object[first], i);
        shouldBe(object[second], String(i));
        shouldBe(object[first + second][0], i);
    }
}
