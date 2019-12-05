//@ runDefault

try {
    const s = (10).toLocaleString().padEnd(2**31-1, 'aa');
    RegExp([s]);
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
