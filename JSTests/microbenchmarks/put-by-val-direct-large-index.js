//@ skip if $memoryLimited
var acc = [];
for (var i = 0; i < 1e6; i++) {
    acc.push({[5e4 + (i % 1e4)]: true});
}
