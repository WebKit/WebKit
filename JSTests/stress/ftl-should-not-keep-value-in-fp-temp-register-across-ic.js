function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

// Keep more doubles alive across the access than the number of FP registers so that
// FTL uses all of them, including the one MacroAssembler uses as fpTempRegister.
const count = 40;

function createTest() {
    let source = "";
    for (let i = 0; i < count; ++i)
        source += `let d${i} = v + ${i + 1};\n`;
    source += "let element = array[index];\n";
    source += "return element";
    for (let i = 0; i < count; ++i)
        source += ` + d${i}`;
    source += ";\n";

    const test = new Function("array", "index", "v", source);
    noInline(test);
    return test;
}

for (const constructor of [Float64Array, Float32Array, Float16Array]) {
    const test = createTest();
    const typedArray = new constructor(8);
    const object = { };
    for (let i = 0; i < 8; ++i) {
        typedArray[i] = i + 0.25;
        object[i] = i + 0.25;
    }

    for (let i = 0; i < testLoopCount; ++i) {
        // Passing the object sometimes makes the access generic, so that FTL emits IC for it.
        const array = (i & 15) ? typedArray : object;
        const v = (i & 1023) + 0.5;
        for (let index = 0; index < 8; ++index)
            shouldBe(test(array, index, v), index + 0.25 + count * v + count * (count + 1) / 2);
    }
}
