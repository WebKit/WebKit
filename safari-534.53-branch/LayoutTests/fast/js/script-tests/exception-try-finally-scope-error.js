description('This test makes sure stack unwinding works correctly in combination with dynamically added scopes');

function gc()
{
    if (this.GCController)
        GCController.collect();
    else
        for (var i = 0; i < 10000; ++i) // Allocate a sufficient number of objects to force a GC.
            ({});
}

var result;
function runTest() {
    var test = "outer scope";
    with({test:"inner scope"})
       (function () { try { throw ""; } finally { result = test; shouldBe("result", '"inner scope"'); return;}})()
}
runTest();

try{
(function() {
    try {
        throw "";
    } catch(y) {
        throw (function(){});
    } finally {
    }
})()
}catch(r){
}

// Just clobber any temporaries
a=({});
a*=a*a*a;

gc();

var successfullyParsed = true;
