//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function benchDouble(n) {
    let sum = 0;
    for (let i = 0; i < n; i++) {
        let x = ((i * 0x9E3779B9) | 0) * 1.5;
        sum += Math.sign(x);
    }
    return sum;
}
noInline(benchDouble);

let result;
for (let i = 0; i < 100; i++)
    result = benchDouble(1e6);
