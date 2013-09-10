description("Ensure that we correctly tearoff the arguments objects when throwing from inlined function");

var fiftiethArguments = null;

function g(a) { if (a === 50) fiftiethArguments = arguments; f(); }
function f() { doStuff();  }
function doStuff() { throw {}; }


for (var i = 0; i < 100; i++) { try {  g(i) } catch (e) { } }

shouldBe("fiftiethArguments[0]", "50");
shouldBe("fiftiethArguments.length", "1");


