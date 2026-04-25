description(
"This tests that a assigning to a readonly property in a static or symbol table throws in strict mode."
);

function testWindowUndefined()
{
    "use strict";
    try {
         window.undefined = 42;
    } catch (e) {
        return e instanceof TypeError;
    }
    return false;
}

function testNumberMAX_VALUE()
{
    "use strict";
    try {
         Number.MAX_VALUE = 42;
    } catch (e) {
        return e instanceof TypeError;
    }
    return false;
}

shouldBeTrue('testWindowUndefined()');
shouldBeTrue('testNumberMAX_VALUE()');
