function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(label + ": expected " + expected + " but got " + actual);
}

function flatten(string) {
    return JSON.parse(JSON.stringify(string));
}

function substringSharingBuffer(string, start, end) {
    let substring = string.substring(start, end);
    /x/.test(substring);
    return substring;
}

for (let length = 0; length <= 130; ++length) {
    let label = "length " + length;
    shouldBe(flatten(";".repeat(length)).indexOf(":"), -1, label);
    shouldBe(flatten("º".repeat(length)).includes(":"), false, label);
    shouldBe(flatten("è".repeat(length)).indexOf("é"), -1, label);

    let surrounded = flatten(":".repeat(16) + ";".repeat(length) + ":".repeat(16));
    shouldBe(substringSharingBuffer(surrounded, 16, 16 + length).indexOf(":"), -1, label);
    shouldBe(surrounded.substring(16, 16 + length).indexOf(":"), -1, label);
    shouldBe(substringSharingBuffer(surrounded, 16, 17 + length).indexOf(":"), length, label);
    shouldBe(substringSharingBuffer(surrounded, 15, 16 + length).indexOf(":"), 0, label);

    for (let position = 0; position < length; ++position) {
        label = "length " + length + " position " + position;
        shouldBe(flatten(";".repeat(position) + ":" + ";".repeat(length - position - 1)).indexOf(":"), position, label);
        shouldBe(flatten(";".repeat(position) + ":".repeat(length - position)).indexOf(":"), position, label);
        shouldBe(flatten("è".repeat(position) + "é" + "ÿ".repeat(length - position - 1)).includes("é"), true, label);
    }
}
