const s1 = (-1).toLocaleString().padEnd(2**31-1, 'aa');
try {
    s1.lastIndexOf();
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";
