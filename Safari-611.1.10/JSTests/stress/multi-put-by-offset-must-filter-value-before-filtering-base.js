//@ runDefault("--collectContinuously=1", "--useConcurrentJIT=0", "--useConcurrentGC=1")

function foo(oo) {
    oo.x = 4;
    oo.y = 4;
    oo.e = oo;
    oo.e = 7;
    oo.f = 8;
}
noInline(foo);

function Foo() {
    foo(this);
}

for (var i = 0; i < 100000; i++) {
    g();
}

function g(){
    foo({f:8});
    new Foo();
    new Foo();
    new Foo();
}
