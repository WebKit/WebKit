function opt(){
    var x = -19278.05 >>> NaN;
    var y = x + -19278.05;
    var z = y >> 0;
    return z;
}

let a = opt();
for(let i = 0; i < 20000; i++)
    opt();
let b = opt();

if (a !== b)
    throw 'Expected ' + a + ' === ' + b
