//@ if $jitTests then runNoCJIT("--useDFGJIT=false", "--useLLInt=false") else skip end

function foo(o) {
    return o.f;
}

var p1 = {};
p1.f = 42;

var crazy = {};
crazy.f = 1;
crazy.g = 2;

var p2 = Object.create(p1);

var crazy = Object.create(p1);
crazy.f = 1;
crazy.g = 2;

function make() {
    return Object.create(p2);
}

for (var i = 0; i < 100; ++i)
    foo(make());

for (var i = 0; i < 10000; ++i)
    p2["i" + i] = i;
p2.f = 43;

for (var i = 0; i < 100; ++i)
    foo({f:24});

var result = foo(make());
if (result != 43)
    throw "Error: bad result: " + result;
