description(
"Test method-check related bugs"
);

function func2() { }

// This test yields PASS, if malloc does not reuse the memory address for the structure of String prototype
function func()
{
    String.prototype.a = function() { }
    String.prototype.b = function() { }

    if (window.GCController)
        GCController.collect();
    else {
        // The following 3 lines cause gc() flush on a Debian
        // Linux machine, but there is no garantee, it works on
        // any other computer. (Not even another Debian Linux)
        // If func2() is not called or a much bigger or lower
        // value than 5000 is chosen, the crash won't happen
        func2()
        for (var i = 0; i < 5000; ++i)
            new Boolean()
    }

    var str = ""
    for (var i = 0; i < 5; ++i)
        str.a()
}

func()
func()

// Test that method caching correctly invalidates (doesn't incorrectly continue to call a previously cached function).
var total = 0;
function addOne()
{
    ++total;
}
function addOneHundred()
{
    total+=100;
}
var totalizer = {
    makeCall: function(callback)
    {
        this.callback = callback;
        this.callback();
    }
};
for (var i=0; i<100; ++i)
    totalizer.makeCall(addOne);
totalizer.makeCall(addOneHundred);
shouldBe('total', '200');

// Check that we don't assert when method_check is applied to a non-JSFunction
for (var i = 0; i < 10000; i++)
    Array.constructor(1);
