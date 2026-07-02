const memories = 128;
const verbose = false;
const initial = 1;

let types = {};

for (let m = 0; m < memories; ++m) {
    let memory = new WebAssembly.Memory({ initial: initial });
    let type = WebAssemblyMemoryMode(memory);
    types[type] = types[type] ? types[type] + 1 : 1;
}

if (verbose) {
    let got = "Got: ";
    for (let p in types)
        got += ` ${types[p]}: ${p}`;
    print(got);
}
