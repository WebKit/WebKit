//@ runDefault("--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=20", "--thresholdForFTLOptimizeAfterWarmUp=40")
//@ runDefault("--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=20", "--thresholdForFTLOptimizeAfterWarmUp=40", "--collectContinuously=1")

function makeFunction(seed) {
    let body = "let r = x | 0;\n";
    for (let i = 0; i < 200; ++i) {
        switch ((i + seed) % 4) {
        case 0:
            body += `if (r & ${1 << (i % 8)}) r = (r + ${i}) | 0; else r = (r ^ ${i * 7}) | 0;\n`;
            break;
        case 1:
            body += `r = (r + o.f${i % 16}) | 0;\n`;
            break;
        case 2:
            body += `o.f${i % 16} = (r & 0xff);\n`;
            break;
        case 3:
            body += `r = (r + a[${i % 32}]) | 0;\n`;
            break;
        }
    }
    body += "return r;";
    return new Function("x", "o", "a", body);
}

function makeObject() {
    let o = {};
    for (let i = 0; i < 16; ++i)
        o["f" + i] = i;
    return o;
}

let functions = [];
for (let i = 0; i < 3; ++i)
    functions.push(makeFunction(i));

let array = [];
for (let i = 0; i < 32; ++i)
    array.push(i);

let garbage = [];
for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    for (let j = 0; j < functions.length; ++j) {
        let first = functions[j](iteration, makeObject(), array);
        let second = functions[j](iteration, makeObject(), array);
        if (first !== second)
            throw new Error(`Bad result at iteration ${iteration}, function ${j}: ${first} vs ${second}`);
    }
    for (let k = 0; k < 32; ++k)
        garbage.push({ a: iteration, b: [k, iteration], c: "s" + k });
    if (garbage.length > 20000)
        garbage = [];
}
