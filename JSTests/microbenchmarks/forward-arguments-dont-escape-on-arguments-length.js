function f1() {
    'use strict';
    if (arguments.length === 1)
        return 25;
    return 42;
}
noFTL(f1);
noInline(f1);

function f2() {
    if (arguments.length === 1)
        return 25;
    return 42;
}
noFTL(f2);
noInline(f2);

class C {
    construct() {
        let x = 1;
        if (arguments.length === 2)
            --x;
        if (x)
            this.x = x; 
    }
}
noFTL(C);
noInline(C);

for (let i = 0; i < 1000000; ++i)
    f1(10, 20);
for (let i = 0; i < 1000000; ++i)
    f2(10, 20);
for (let i = 0; i < 1000000; ++i)
    new C(10, 20);
