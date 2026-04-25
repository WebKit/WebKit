let x = new Uint8Array(10000);
let y = new Uint8ClampedArray(10000);

let start = Date.now();
for (let i = 0; i < 10000; ++i) {
    x.set(y);
    y.set(x);
}
if (false)
    print(Date.now() - start);
