// This test should not crash.

let x = undefined;

function foo(w, a0, a1) {
    var r0 = x % a0; 
    var r1 = w ^ a1; 

    var r4 = 3 % 7; 

    var r6 = w ^ 0;
    var r7 = r4 / r4; 
    var r9 = x - r7; 
    a1 = 0 + r0;

    var r11 = 0 & a0; 
    var r12 = r4 * a1; 
    var r7 = r11 & a0; 

    var r15 = r11 | r4; 
    var r16 = 0 & r1; 
    var r20 = 5 * a0; 

    var r2 = 0 + r9;
    var r26 = r11 | r15; 
    var r29 = r16 + 0;
    var r29 = r28 * r1; 
    var r34 = w / r12; 

    var r28 = 0 / r7;
    var r64 = r20 + 0;
    var r65 = 0 + r6;

    return a1;
}
noInline(foo);

for (var i = 0; i < 1886; i++)
    foo("q");

