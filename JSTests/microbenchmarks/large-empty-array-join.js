//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ $skipModes << :lockdown if $memoryLimited or $architecture == "mips"
const array = [];
for (let i = 0; i < 100; ++i)
    array.push(new Array(1e6).join('—' + i));
