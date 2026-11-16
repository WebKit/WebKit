function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

function splitAndCaptureStatics(regexp, input, limit) {
    /ZZZ/.exec("xxxZZZyyy");
    const result = input.split(regexp, limit);
    result.push(RegExp.input, RegExp.lastMatch, RegExp.leftContext, RegExp.rightContext, regexp.lastIndex);
    return result.join("|");
}

function compare(source, flags, inputs, limits) {
    const regexp = new RegExp(source, flags);
    const reference = new RegExp("(?:" + source + ")", flags);
    regexp.lastIndex = reference.lastIndex = 3;
    for (const input of inputs) {
        for (const limit of limits) {
            const actual = splitAndCaptureStatics(regexp, input, limit);
            const expected = splitAndCaptureStatics(reference, input, limit);
            if (actual !== expected)
                throw new Error("/" + source + "/" + flags + " " + JSON.stringify(input) + " " + limit + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
        }
    }
}

const inputs = [""];
for (let i = 0; inputs[i].length < 5; ++i) {
    for (const character of ["a", "\r", "\n"])
        inputs.push(inputs[i] + character);
}
const shortInputs = inputs.slice(0, 121);
for (const input of inputs.slice(0, 40))
    inputs.push("\u3042" + input, input + "\u3042");
for (const length of [7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65]) {
    for (const separator of ["\n", "\r\n", "\r", "\r\r\n", "\n\r"]) {
        for (const fill of ["x", "\u3042"])
            inputs.push(fill.repeat(length) + separator + fill.repeat(length) + separator);
    }
}

compare("\\r?\\n", "", inputs, [undefined, 0, 1, 2, 5, -1]);

for (const flags of ["g", "ds"])
    compare("\\r?\\n", flags, shortInputs, [undefined, 2]);

for (const source of ["\\r\\n", "\\r??\\n", "\\r*\\n", "\\r+\\n", "\\r{0,2}\\n", "\\n?\\n", "a?\\n", "\\r?\\r", "\\r?a", "\\r?\\n?", "\\r?\\n*", "\\r?\\n+", "\\r?\\n{2}", "\\r?", "\\r?\\n\\n", "\\r?\\na", "\\r?\\n|a", "(\\r?\\n)", "^\\r?\\n", "\\r?\\n$"])
    compare(source, "", shortInputs, [undefined, 2]);

function split(input, limit) {
    return input.split(/\r?\n/, limit);
}
noInline(split);

function symbolSplit(input, limit) {
    return /\r?\n/[Symbol.split](input, limit);
}
noInline(symbolSplit);

function test(splitFunction, mixed, wide) {
    shouldBe(splitFunction(mixed).join("|"), "a|b|c\rd\r|e|\rf|", "mixed");
    shouldBe(RegExp.lastMatch, "\n", "lastMatch");
    shouldBe(RegExp.leftContext, "a\nb\r\nc\rd\r\r\ne\n\rf", "leftContext");
    shouldBe(RegExp.rightContext, "", "rightContext");

    shouldBe(splitFunction(wide, 2).join("|"), "\u3042|\u3044", "wide");
    shouldBe(RegExp.lastMatch, "\n", "lastMatch with limit");
    shouldBe(RegExp.leftContext, "\u3042\r\n\u3044", "leftContext with limit");
    shouldBe(RegExp.rightContext, "\u3046\r", "rightContext with limit");
}

const mixed = "a\nb\r\nc\rd\r\r\ne\n\rf\n";
const wide = "\u3042\r\n\u3044\n\u3046\r";
const nonAtomMixed = mixed.split("").join("");
const nonAtomWide = wide.split("").join("");

for (let i = 0; i < testLoopCount; ++i) {
    const splitFunction = i & 1 ? split : symbolSplit;
    if (i & 2)
        test(splitFunction, mixed, wide);
    else
        test(splitFunction, nonAtomMixed, nonAtomWide);
}
