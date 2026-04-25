//@requireOptions("--thresholdForFTLOptimizeSoon=0", "--validateAbstractInterpreterState=1")
function foo() {
    let a = Object;
    let b = Object;
    let c = Array.prototype;
    for (let i = 0; i < 10; i++) {
        for (let j = 0; j < 100; j++) {
            let xs = [0, 1];
            for (let x in xs) { }
        }
    }
}

for (let i = 0; i < 100; i++) {
    foo();
}
