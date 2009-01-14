description(
'Tests to make sure we do not gc the constants contained by functions defined inside eval code.  To pass we need to not crash.'
);

function gc()
{
    if (this.GCController)
        GCController.collect();
    else
        for (var i = 0; i < 10000; ++i) // Allocate a sufficient number of objects to force a GC.
            ({});
}

evalStringTest = "'test'";
evalString = "function f() { shouldBe(\"'test'\", evalStringTest) }; f()";
function doTest() {
    eval(evalString);
}
doTest();
gc();

// Scribble all over the registerfile and c stacks
a={};
a*=({}*{}+{}*{})*({}*{}+{}*{})+({}*{}+{}*{});
[[[1,2,3],[1,2,3],[1,2,3]],[[1,2,3],[1,2,3],[1,2,3]],[[1,2,3],[1,2,3],[1,2,3]]];
gc();
a={};
a*=({}*{}+{}*{})*({}*{}+{}*{})+({}*{}+{}*{});
[[[1,2,3],[1,2,3],[1,2,3]],[[1,2,3],[1,2,3],[1,2,3]],[[1,2,3],[1,2,3],[1,2,3]]];
gc();
a={};
a*=({}*{}+{}*{})*({}*{}+{}*{})+({}*{}+{}*{});
[[[1,2,3],[1,2,3],[1,2,3]],[[1,2,3],[1,2,3],[1,2,3]],[[1,2,3],[1,2,3],[1,2,3]]];

doTest();
var successfullyParsed = true;
