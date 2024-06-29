function foo(a,b,c) { let x = a | 0; let y = b | 0; let z = c &15;
z = (x<<y)^(x<<(y&0x10ff)); let r = z^0xf01;
let s = z^0xf1f;
return (((a>>>r)<<s)>>s);
}
let LEN = 100000000-1;
let res = 0;
res = foo((LEN&127),456,789);

if (res != -1)
    throw "Wrong result: " + res

for (let i = 0; i <= LEN; i++) res = foo((i&127),456,789);

if (res != -1)
    throw "Wrong result: " + res