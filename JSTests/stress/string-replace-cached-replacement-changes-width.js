function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + JSON.stringify(actual).slice(0, 80));
}

function flat8Bit(content) {
    return content.split("").join("");
}

function flat16Bit(content) {
    let flat = String.fromCharCode(0x3042) + content;
    Symbol(flat);
    let wide = flat.slice(1);
    Symbol(wide);
    return wide;
}

let table = {};
let filler = ".".repeat(2048);

function test(regExp, name, makeAtom, makeReplacement) {
    table[makeAtom(name)] = 1;
    let replacement = makeReplacement(name);
    let count = 0;
    let result = flat8Bit("x" + filler + "x" + filler + "x").replace(regExp, function () {
        if (++count === 1)
            return replacement;
        table[replacement] = 2;
        return count;
    });
    shouldBe(count, 3);
    shouldBe(result, name + filler + "2" + filler + "3");
}

let index = 0;
for (let regExp of [/x/g, /(x)/g]) {
    test(regExp, "property-name-" + index++, flat16Bit, flat8Bit);
    test(regExp, "property-name-" + index++, flat8Bit, flat16Bit);
}
