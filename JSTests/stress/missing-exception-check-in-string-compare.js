const s1 = (-1).toLocaleString().padEnd(2**31-1, 'aa');
const s2 = (-1).toLocaleString().padEnd(2**31-1, 'aa');
try {
    // Force evaluation of Rope
    s1 == s2;
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";
