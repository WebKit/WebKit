description(
"Tests what happens when we OSR exit on an inlined prototype access due to a change in the prototype chain."
);

function foo(o) {
    return o.g.f;
}

function Thingy() {
}

var myProto = {f:42};

Thingy.prototype = myProto;

for (var i = 0; i < 200; ++i) {
    if (i == 150)
        myProto.g = 67;
    shouldBe("foo({g:new Thingy()})", "42");
}

