const a = new Float32Array(1);
const b = [2];
function test() {
    function foo() { }
    for (let i = 0; i < 100; i++) {
        switch (foo) {
            default:
                for (const j of a) { }
            case 0:
            case 1:
            case 1:
        }
    }
}

for (let i = 0; i < testLoopCount; i++) {
    test();
}
