//@ runDefault("--useConcurrentJIT=0")

forEach({ length: 5 }, function() {
    for (var i = 0; i < 10; i++) {
        forEach([1], function() {});
    }
});

function forEach(a, b) {
    for (var c = 0; c < a.length; c++)
        b();
}
