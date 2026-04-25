//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function exponentiate(x, y) { return x ** y; }
noInline(exponentiate);

for (let i = -50; i <= 50; i += 0.1)
    for (let j = 0; j <= 1000; j++) 
        exponentiate(i, j);
