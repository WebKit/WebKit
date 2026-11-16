function chain(n, w) {
    let x = 1.0;
    for (let i = 0; i < n; ++i)
        x = -x * w;
    return x;
}
noInline(chain);

let result = 0;
for (let i = 0; i < 600; ++i)
    result += chain(100000, 1.0);

if (result !== 600)
    throw new Error(`bad result: ${result}`);
