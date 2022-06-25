//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0", "--collectContinuously=1")

let thing = []

function bar(x) {
    thing.push(x);
}

function foo() {
    let hello = function () {
        let tmp = 1;
        return function (num) {
            if (tmp) {
                if (num.length) {
                }
            }
        };
    }();

    bar();
    for (j = 0; j < 10000; j++) {
        if (/\s/.test(' ')) {
            hello(j);
        }
    }
}

for (let i=0; i<100; i++) {
    foo();
}
