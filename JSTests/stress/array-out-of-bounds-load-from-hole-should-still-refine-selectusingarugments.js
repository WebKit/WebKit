//@runDefault("--useConcurrentJIT=false", "--useFTLJIT=0", "--jitPolicyScale=0")
//try{runString(`
function opt(a1,a1,a3){
    function foo18fb_1() {
        try {
            a1.forEach(function(a4) {
                try {
                    a1[Symbol.isConcatSpreadable] = false;
                } catch(a1457) {};
            });
        } catch(a1458) {};
    }
    function bar18fb_1() {
        return foo18fb_1();
    }
    bar18fb_1();
}
try{opt(1.337,([0.1, ,,,,,,,,,,,,,,,,,,,,,, 6.1, 7.1, 8.1]),1.337)}catch(y){}

runString(`
    function opt1(a1){
        a1.forEach(function(a3) {
        });;
    }
    try{opt1(([0.1, ,,,,,,,,,,,,,,,,,,,,,, 6.1, 7.1, 8.1]))}catch(y){}
`);
