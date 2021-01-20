description(
"Tests what happens when you use a function as a variable, and you put things into it that aren't constants."
);

function run_tests(a) {
    function x() {
      return 3;
    }  

    var x = a;
    return x + x;
}

for(var i=0; i<100; ++i)
    shouldBe("run_tests(13)", "26");

