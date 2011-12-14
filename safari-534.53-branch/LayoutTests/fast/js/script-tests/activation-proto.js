description(

"This test checks that activation objects (the local scope for a function) don't have the special __proto__ property that lets you get and set a normal object's prototype. This is important because the impossibility of swizzling activation object prototype chains allows various optimizations."

);

shouldBe("(function() { __proto__.testVariable = 'found'; return window.testVariable; })()", "'found'");

successfullyParsed = true;
