//@ skip if $memoryLimited

try {
    bar_693 = '2.3023e-320';
    foo_508 = bar_693.padEnd(2147483620, 1);
    var newInstance = new foo_508(1, 2);
} catch {
    // Allocating this error shouldn't crash us.
}
