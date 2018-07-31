//@ skip if $memoryLimited
const s = "a".padStart(0x80000000 - 1);
try {
    JSON.stringify(s);
} catch (e) {}
