description(
"Tests that DFG inlining does not brak function.arguments.caller."
);

var callCount = 0;

var resultArray = []

function throwError() {
   throw {};
}
var object = {
   nonInlineable : function nonInlineable() {
       if (0) return [arguments, function(){}];
       if (++callCount == 9999999) {
           var f = nonInlineable;
           while (f) {
               resultArray.push(f.name);
               f=f.arguments.callee.caller;
           }
       }
   },
   inlineable : function inlineable() {
       this.nonInlineable();
   }
}
function makeInlinableCall(o) {
   for (var i = 0; i < 10000; i++)
       o.inlineable();
}

function g() {
    var j = 0;
    for (var i = 0; i < 1000; i++) {
        j++;
        makeInlinableCall(object);
    }
}
g();

shouldBe("resultArray.length", "4");
shouldBe("resultArray[3]", "\"g\"");
shouldBe("resultArray[2]", "\"makeInlinableCall\"");
shouldBe("resultArray[1]", "\"inlineable\"");
shouldBe("resultArray[0]", "\"nonInlineable\"");

