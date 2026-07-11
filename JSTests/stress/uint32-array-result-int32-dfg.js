//@ runDefault("--useConcurrentJIT=false", "--useFTLJIT=false", "--jitPolicyScale=0.1")
// See rdar://176792844 for additional details on this test.

let a = new Uint32Array(4);
a[0] = 42;

let dummy = [1, 2, 3]; dummy[0] = 1; dummy[0] = {};

const sBodyArgs = ['a', 'idx', 'flag', 'intArr',
"\n" +
"    let i = idx;\n" +
"    if (flag) i = 0.5;\n" +
"    let v = a[i];\n" +
"    let d = v - 0.5;\n" +
"    intArr[0] = v;\n" +
"    return d;\n"];

let f1 = new Function(...sBodyArgs);
noInline(f1);

let intArr1 = [1, 2, 3]; intArr1[0] = 1;
for (let k = 0; k < 100; k++)
    f1(a, 0, false, intArr1);

for (let k = 0; k < 110; k++)
    f1(a, 100, false, intArr1);

let evict = new Function('z', 'return z'); evict(0);

let f2 = new Function(...sBodyArgs);
noInline(f2);

let intArr2 = [1, 2, 3]; intArr2[0] = 1;

for (let k = 0; k < 100; k++)
    f2(a, 0, false, intArr2);

f2(a, 0, false, intArr2);

if (intArr2[0] !== 42)
    throw new Error("incorrect value " + intArr2[0]);
