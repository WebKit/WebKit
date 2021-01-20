//@ if $memoryLimited then skip else runDefault end

try {
    const s = "a".padStart(0x80000000 - 1);
    JSON.stringify(s);
} catch(e) {
    if (e != "RangeError: Out of memory")
        throw e;
}

