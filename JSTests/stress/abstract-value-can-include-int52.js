//@ runDefault("--useRandomizingFuzzerAgent=1", "--jitPolicyScale=0", "--useConcurrentJIT=0", "--useConcurrentGC=0")

function foo(n) {
    while (n) {
        n >>>= 1;
    }
    return ''[0];
}
var indexP;
var indexO = 0;
for (var index = 0; index <= 100; index++) {
    if (index < 8 || index > 60 && index <= 65 || index > 1234 && index < 1234) {
        let x = foo(index);
        if (parseInt('1Z' + String.fromCharCode(index), 36) !== 71) {
            if (indexO === 0) {
                indexO = 0;
            } else {
                if (index - indexP) {
                    var hexP = foo(indexP);
                    index - index
                    index = index;
                }
            }
            indexP = index;
        }
    }
}
