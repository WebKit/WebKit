//@ skip if $memoryLimited
//@ runDefault("--validateExceptionChecks=1")
try {
    btoa('\u0100'.padStart(2 ** 31-1));
} catch { }
try {
    atob('\u0100'.padStart(2 ** 31-1));
} catch { }
