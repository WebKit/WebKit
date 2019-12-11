//@ runDefault("--useConcurrentJIT=0", "--usePutStackSinking=0")

function foo() {
    var args1 = function () {
        return arguments;
    }();
    var args2 = function () {
        var result = arguments;
        result.length = 1;
        return result;
    }(1);
    for (var i = 0; i < 10000000; ++i) {
        args1.length;
        args2.length;
    }
}
foo();
foo();
foo();
