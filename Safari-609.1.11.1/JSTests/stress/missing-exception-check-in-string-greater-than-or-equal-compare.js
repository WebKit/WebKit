const s1 = (-1).toLocaleString().padEnd(2**31-1, 'aa');
try {
    '' >= s1;
} catch (e) {
    exception = e;
}

if (exception != 'Error: Out of memory')
    throw "FAILED";

exception = undefined;

const s2 = (-1).toLocaleString().padEnd(2**31-1, 'aa');
try {
    s2 >= '';
} catch (e) {
    exception = e;
}

if (exception != 'Error: Out of memory')
    throw "FAILED";
