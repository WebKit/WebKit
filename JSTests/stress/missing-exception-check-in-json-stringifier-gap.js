//@ skip if $memoryLimited
//@ runDefault

const s0 = (10).toLocaleString();
const s1 = s0.padStart(2**31-1, 'aa');
try {
    JSON.stringify(0, undefined, s1);
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
