//@ runFTLNoCJIT

(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        const v = Array & 1 ? v : 1;
    }
}());


