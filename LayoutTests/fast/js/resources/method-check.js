description(
"This test yields PASS, if malloc does not reuse the memory address for the structure of String prototype"
);

function func2() { }

function func()
{
    String.prototype.a = function() { }
    String.prototype.b = function() { }

    // there is no gc() call in LayoutTests!

    // The following 3 lines cause gc() flush on a Debian
    // Linux machine, but there is no garantee, it works on
    // any other computer. (Not even another Debian Linux)
    // If func2() is not called or a much bigger or lower
    // value than 5000 is chosen, the crash won't happen
    func2()
    for (var i = 0; i < 5000; ++i)
        new Boolean()

    var str = ""
    for (var i = 0; i < 5; ++i)
        str.a()
}

func()
func()

shouldBeTrue("true");
var successfullyParsed = true;

