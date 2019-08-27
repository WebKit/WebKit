var exception;
try {
    Function('a'.repeat(2147483623));
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";

