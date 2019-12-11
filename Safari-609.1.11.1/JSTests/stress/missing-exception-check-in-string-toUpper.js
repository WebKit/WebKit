const s1 = (-1).toLocaleString().padEnd(2**31-1, 'aa');
try {
    s1.toUpperCase();
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
