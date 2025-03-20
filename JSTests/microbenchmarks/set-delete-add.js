//@ skip if $memoryLimited
var map = new Set();

for (var i = 0; i < 1000; i++) {
    for (var j = 0; j < 1000; j++) {
        map.delete(j);
        map.add(j);
    }
}
