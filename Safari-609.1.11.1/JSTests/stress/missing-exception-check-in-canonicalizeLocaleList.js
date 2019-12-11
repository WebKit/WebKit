try {
const s1 = (-1).toLocaleString().padEnd(2**31-1, 'aa');
'a'.toLocaleLowerCase(s1);
} catch (e) { exception = e }
if (exception != "Error: Out of memory")
    throw "FAILED";

try {
const s1 = (-1).toLocaleString().padEnd(2**31-1, 'aa');
'a'.toLocaleUpperCase(s1);
} catch (e) { exception2 = e }
if (exception2 != "Error: Out of memory")
    throw "FAILED";

try {
const s1 = (-1).toLocaleString().padEnd(2**31-1, 'aa');
'a'.localeCompare('b', s1);
} catch (e) { exception3 = e }
if (exception3 != "Error: Out of memory")
    throw "FAILED";
