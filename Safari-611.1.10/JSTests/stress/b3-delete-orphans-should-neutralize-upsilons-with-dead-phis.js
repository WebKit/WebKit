//@ runFTLNoCJIT

(function () {
    for (var i = 0; i < 1000000; ++i) {
        const v = Array & 1 ? v : 1;
    }
}());


