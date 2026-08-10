function testLower(string) {
    return string.toLowerCase();
}
noInline(testLower);

function testUpper(string) {
    return string.toUpperCase();
}
noInline(testUpper);

function makeString(base) {
    return base + "";
}
noInline(makeString);

const prefixLengths = [64, 256, 1024, 4096, 16384];

const lowerInputs = prefixLengths.map(len => makeString("a".repeat(len) + "Z"));
const upperInputs = prefixLengths.map(len => makeString("A".repeat(len) + "z"));

for (let i = 0; i < 1e4; ++i) {
    for (const s of lowerInputs)
        testLower(s);
    for (const s of upperInputs)
        testUpper(s);
}
