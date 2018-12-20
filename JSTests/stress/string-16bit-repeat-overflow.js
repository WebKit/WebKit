var exception;
try {
    print('\ud000'.repeat(2**30));
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
