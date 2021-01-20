function testModifyLength() {
    "use strict";

    arguments.length = 10;
    return arguments.length;
}
noInline(testModifyLength);

function testAddOtherProperty() {
    "use strict";

    arguments.foo = 1;
    return arguments.length;
}
noInline(testAddOtherProperty);

function testAddOtherPropertyInBranch() {
    "use strict";

    if (arguments[0] % 2)
        arguments.foo = 1;
    return arguments.length;
}
noInline(testAddOtherPropertyInBranch);

for (i = 0; i < 100000; i++) {
    if (testModifyLength(1) !== 10)
        throw "bad";
    if (testAddOtherProperty(1) !== 1)
        throw "bad";
    if (testAddOtherPropertyInBranch(i) !== 1)
        throw "bad";
}
