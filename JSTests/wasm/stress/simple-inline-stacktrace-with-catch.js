var wasm_code = read('simple-inline-stacktrace-with-catch.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
let throwCounter = 0
let throwAt = 0
var wasm_instance = new WebAssembly.Instance(wasm_module, { a: { doThrow: () => {
    if (throwCounter == throwAt)
        throw new Error()
    ++throwCounter
} } });
var f = wasm_instance.exports.main;
let readMem = new Int32Array(wasm_instance.exports.mem.buffer)
const iterCount = 9

function verifyStack(stack, e) {
    let str = e.stack.toString()
    let trace = str.split('\n')
    let expected = ["*"]
    for (let i of stack)
        expected.push(`${i}@wasm-function[${i}]`)
    expected = expected.concat(["*"])

    if (trace.length != expected.length)
        throw "unexpected length, got: \n" + str + "\nExpected:\n" + expected.join("\n")
    for (let i = 0; i < trace.length; ++i) {
        if (expected[i] == "*")
            continue
        if (expected[i] != trace[i].trim())
            throw "mismatch at " + i + ", got: \n" + str + "\nExpected:\n" + expected.join("\n")
    }
}

function verifyMem(expected) {
    if (readMem[0] != expected)
        throw "Expected " + expected + ", got " + readMem[0]
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 0
        f()
    } catch (e) {
        verifyStack([16], e)
    }
   verifyMem(0)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 1
        f()
    } catch (e) {
        verifyStack([15, 16], e)
    }
   verifyMem(0)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 2
        f()
    } catch (e) {
        verifyStack([15, 16], e)
    }
   verifyMem(0)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 3
        f()
    } catch (e) {
        verifyStack([15, 16], e)
    }
   verifyMem(0)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 4
        f()
    } catch (e) {
        verifyStack([15, 16], e)
    }
   verifyMem(i + 1)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 5
        f()
    } catch (e) {
        verifyStack([15, 16], e)
    }
   verifyMem(i + 1)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 6
        f()
    } catch (e) {
        verifyStack([15, 16], e)
    }
   verifyMem(i + 1)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 7
        f()
    } catch (e) {
        verifyStack([15, 16], e)
    }
   verifyMem(i + 1)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 8
        f()
    } catch (e) {
        verifyStack([14, 15, 16], e)
    }
   verifyMem(i + 1)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 9
        f()
    } catch (e) {
        verifyStack([11, 12, 13, 14, 15, 16], e)
    }
   verifyMem(i + 1)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 10
        f()
    } catch (e) {
        verifyStack([11, 12, 13, 14, 15, 16], e)
    }
   verifyMem(2*i + 2)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 11
        f()
    } catch (e) {
        verifyStack([14, 15, 16], e)
    }
   verifyMem(2*i + 2)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 12
        f()
    } catch (e) {
        verifyStack([16], e)
    }
   verifyMem(2*i + 2)
}

readMem[0] = 0
for (let i = 0; i < iterCount; ++i) {
    try {
        throwCounter = 0
        throwAt = 13
        f()
    } catch (e) {
        verifyStack([16], e)
    }
   verifyMem(2*i + 2)
}
