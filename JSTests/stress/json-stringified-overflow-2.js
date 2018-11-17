//@ if $memoryLimited then skip else runDefault end

try {
    const s = "a".padStart(0x80000000 - 1);
    JSON.stringify(s);
} catch(e) {
    if (e != "Error: Out of memory")
        throw e;
}

