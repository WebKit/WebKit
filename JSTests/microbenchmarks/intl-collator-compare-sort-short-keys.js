function makeKeys(count)
{
    let syllables = ["ka", "ri", "to", "Mo", "su", "Na", "be", "lo", "Ze", "pi", "du", "fa", "ge", "ho", "Ji", "wu"];
    let keys = [];
    let seed = 1;
    for (let i = 0; i < count; ++i) {
        let key = "";
        let length = 4 + (i % 5);
        for (let j = 0; j < length; ++j) {
            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            key += syllables[seed % syllables.length];
        }
        keys.push(key);
    }
    return keys;
}

function test(collator, keys, n) {
    let checksum = 0;
    for (let i = 0; i < n; ++i) {
        let sorted = keys.slice().sort(collator.compare);
        checksum += sorted[0].length + sorted[sorted.length - 1].length;
    }
    return checksum;
}

let collator = new Intl.Collator("en");
let keys = makeKeys(500);
let sorted = keys.slice().sort(collator.compare);
for (let i = 1; i < sorted.length; ++i) {
    if (collator.compare(sorted[i - 1], sorted[i]) > 0)
        throw new Error("Bad order at " + i);
}

let expected = (sorted[0].length + sorted[sorted.length - 1].length) * 3000;
let result = test(collator, keys, 3000);
if (result !== expected)
    throw new Error("Bad result: " + result);
