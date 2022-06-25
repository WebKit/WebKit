//@ runDefault("--jitPolicyScale=0--jitPolicyScale=0")

// This test should not crash.

function f_0() {
    var v_4 = 1;
    var v_5 = 'a';
    while (v_4 < 256) {
        v_4 <<= 1;
    }
    return v_4;
}
function f_1(v_1) {
    var sum = 0;
    for (var i = 0; i < 1000; i++) {
        for (var j = 0; j < 4; j++) {
            sum += v_1();
        }
    }
    return sum;
}

let hello;
for (var i=0; i<1000; i++) {
    hello = f_1(f_0);
}
