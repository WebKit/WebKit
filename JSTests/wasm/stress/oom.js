const verbose = false;

// Use a full 4GiB so that exhaustion is likely to occur faster. We're not
// guaranteed that we'll get that much virtually allocated to the memory so we
// can't actually check that exhaustion occurs at a particular number of
// memories.
const maximumPages = 65536;

let memories = [];
try {
    while (true) {
        let m = new WebAssembly.Memory({ initial: 64, maximum: maximumPages });
        memories.push(m);
        if (verbose)
            print(`${WebAssemblyMemoryMode(m)} ${memories.length}`);
    }
} catch (e) {
    if (verbose)
        print(`Caught: ${e}`);
    if (e.message !== "Out of memory")
        throw new Error(`Expected an out of memory error, got ${e} of type ${typeof e}`);
}
