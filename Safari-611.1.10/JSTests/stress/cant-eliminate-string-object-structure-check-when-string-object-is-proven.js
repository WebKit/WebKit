//@ runDefault("--forceEagerCompilation=1", "--useConcurrentJIT=0")

function foo(x) {
    x.toString();
}

var a = new String();
a.valueOf = 0
for (var i = 0; i < 5; i++) {
    foo(a)
}
