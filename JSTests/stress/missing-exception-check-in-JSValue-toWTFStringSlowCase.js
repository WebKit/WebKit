//@ skip if $memoryLimited
//@ runDefault

try {
    RegExp({toString: ()=> ''.padEnd(2**31-1, 10 .toLocaleString()) });
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
