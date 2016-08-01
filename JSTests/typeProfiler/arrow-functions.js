load("./driver/driver.js");

let foo = (x) => x;
let bar = abc => abc;
let baz = abc => { return abc; };
let jaz = abc => { };

function wrapper(b) {
    let baz = (x) => x;
    baz(b);

    let foo = yyy => yyy;
    foo(b);
}

// ====== End test cases ======

foo(20);
var types = returnTypeFor(foo);
assert(types.globalTypeSet.displayTypeName === T.Integer, "Function 'foo' should return 'Integer'");

bar("hello");
types = returnTypeFor(bar);
assert(types.globalTypeSet.displayTypeName === T.String, "Function 'bar' should return 'String'");

baz("hello");
types = returnTypeFor(baz);
assert(types.globalTypeSet.displayTypeName === T.String, "Function 'baz' should return 'String'");

jaz("hello");
types = returnTypeFor(jaz);
assert(types.globalTypeSet.displayTypeName === T.Undefined, "Function 'jaz' should return 'Undefined'");

wrapper("hello");
types = findTypeForExpression(wrapper, "x)"); 
assert(types.instructionTypeSet.displayTypeName === T.String, "Parameter 'x' should be 'String'");

types = findTypeForExpression(wrapper, "yyy =>");
assert(types.instructionTypeSet.displayTypeName === T.String, "Parameter 'yyy' should be 'String'");
