//@ memoryHog!

let ar = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 1073741824));

// A freshly allocated buffer is all zeros, so it is already sorted. Detecting that needs no scratch
// allocation, so this succeeds rather than running out of memory.
var exception;
try {
    ar.sort();
} catch (e) {
    exception = e;
}

if (exception !== undefined)
    throw "FAILED: expected no exception for an already sorted array, got " + exception;

// Swapping neighbours leaves one descending pair, so sorting now has to allocate.
ar[0] = 1;
ar[1] = 0;

// No comparator version.
exception = undefined;
try {
    ar.sort();
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED: " + exception;

// With comparator version.
exception = undefined;
try {
    ar.sort((x, y) => { return x > y });
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED: " + exception;
