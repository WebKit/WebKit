//@ runDefault("--useMaximalFlushInsertionPhase=1", "--useConcurrentJIT=0")
function bar(c) {
    if (c > 1) {
        bar(parseInt(c * 2))
    }
    c *= 2;
}

function foo() {
    var i = 0;
    if (''.match(/^/)) {
        while(i < 1e5) {
            bar(2);
            ++i;
        }
    }
}

foo();
