description(
"Tests what happens when you use a function as a variable on one control flow path, and use it normally on another, and then do something that depends on its structure."
);

var myGlobalVar;

function run_tests(p, z) {
    function x() {
      return 3;
    }  

    if (p) {
        x = z;
        myGlobalVar = x.f;
    } else
        myGlobalVar = x;
    return x.f + x.f;
}

shouldBe("run_tests(false, {f:42})", "0/0");

for(var i=0; i<1000; ++i)
    shouldBe("run_tests(true, {f:42})", "84");

shouldBe("run_tests(false, {f:42})", "0/0");

