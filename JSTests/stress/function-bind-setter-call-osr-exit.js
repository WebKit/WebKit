myObj = {
    val: 1
}

var result = 0;
function bar(a, idx)
{
    "use strict";

    if (idx == 9900)
        myObj.dfgOSR = "Test";

    if (idx == 199900)
        myObj.ftlOSR = "Test";

    return myObj.val + a;
}

var counter = 0;
function foo(a)
{
    "use strict";

    result = bar(a, counter++);
}

boundFoo = foo.bind(null, 41);

var object = {};
Object.defineProperty(object, 'setter', {
    set: boundFoo,
});

function test()
{
    for (var i = 0; i < 200000; i++) {
        object.setter = i;
        if (result != 42)
            testFailed("Function returned " + got + " but expected 42!");
    }
}

noInline(test);

test();
