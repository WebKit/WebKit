//@ skip if $memoryLimited
var string = "\x00".repeat(715827883);
try {
    JSON.stringify(string);
} catch { }
try {
    JSON.stringify(string);
} catch { }
