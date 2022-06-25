//@ skip if $memoryLimited
//@ runDefault

try {
    JSON.parse('a'.repeat(2**31-25));
} catch { }
