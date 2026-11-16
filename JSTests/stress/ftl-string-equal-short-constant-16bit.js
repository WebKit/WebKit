//@ requireOptions("--useDollarVM=1")

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected + " " + message);
}

const constants = ["", "a", "if", "for", "this", "while", "return", "default", "function", "debugger1", "instanceof", "abcdefghijk", "abcdefghijkl", "abcdefghijklm", "abcdefghijklmn", "abcdefghijklmno", "abcdefghijklmnop",
    "あ", "aあ", "あいう", "abcあ", "あいうえお", "abcdefgあ", "あいうえおかきくけ", "abcdefghijklmnoあ", "あいうえおかきくけこさしすせそた"];

let comparators = [];
for (const constant of constants)
    comparators.push(new Function("s", `return s === ${JSON.stringify(constant)};`));

function variants(text) {
    let result = [];
    result.push(text);
    result.push($vm.make16BitStringIfPossible(text));
    result.push(("x" + text + "y").slice(1, 1 + text.length));
    result.push(("あ" + text + "い").slice(1, 1 + text.length));
    result.push($vm.make16BitStringIfPossible("xx" + text + "yy").substring(2, 2 + text.length));
    return result;
}

function inputsFor(constant) {
    let inputs = variants(constant);
    inputs.push(...variants("q".repeat(constant.length)));
    if (constant.length) {
        for (let i = 0; i < constant.length; ++i) {
            let changed = constant.slice(0, i) + String.fromCharCode(constant.charCodeAt(i) + 1) + constant.slice(i + 1);
            inputs.push(...variants(changed));
            let wide = constant.slice(0, i) + String.fromCharCode(constant.charCodeAt(i) + 0x100) + constant.slice(i + 1);
            inputs.push(...variants(wide));
        }
        inputs.push(...variants(constant + "z"));
        inputs.push(...variants(constant.slice(1)));
    }
    return inputs;
}

const ownInputs = constants.map(inputsFor);
const inputs = ownInputs.map((own, i) => own.concat(ownInputs[(i + 1) % ownInputs.length]));

for (let iteration = 0; iteration < testLoopCount / 100; ++iteration) {
    for (let i = 0; i < comparators.length; ++i) {
        const comparator = comparators[i];
        const constant = constants[i];
        const candidates = inputs[i];
        for (let j = iteration % 7; j < candidates.length; j += 7)
            shouldBe(comparator(candidates[j]), candidates[j] == constant, JSON.stringify(candidates[j]) + " vs " + JSON.stringify(constant));
    }
}
