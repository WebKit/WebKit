description(
"Tests that we use the correct profiling accessType for the case we end up compiling/patching."
);

function L_() {
    this.__defineGetter__("mg", function() { return 1365109051943.000000; });
};

function Q2() {};
Q2.mg = 42;

var f = function (Sk){ if(Sk instanceof L_){ return Sk.mg; }else if(Sk instanceof Q2){ return Sk.mg; }else{ return Number(Sk); } }

for (var i = 1; i < 200; i++) {
    if (i % 30 == 0)
        shouldBe("f(new L_())", "1365109051943.000000");
    else
        shouldBe("f(129681120)", "129681120");
}

