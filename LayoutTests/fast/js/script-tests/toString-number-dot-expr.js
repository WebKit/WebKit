description(
"This test checks that toString() round-trip on a function that has a expression of form 4..x does not lose its meaning."
+ " The expression accesses the property 'x' from number '4'."
);

// construct same test-case for different kinds of number literals. the switch is used to avoid
// individual returns getting optimized away (if the interpreter would do dead code elimination)

// testcase for number literal with decimal point, i.e '4.'
function f1(a) {
    switch(a) {
    case "member":
        return 4..x;
    case "arrayget":
        return 4.["x"];
    case "constr":
        return 4.();
    case "funccall":
        return 4..f();
    case "parenfunccall":
        return (4..x)();
    case "assignment":
        return 4..x = 33;
    case "assignment2":
        return 4..x >>>= 1;
    case "prefix":
        return ++4..x;
    case "postfix":
        return 4..x++;
   case "delete":
        delete 4..x;
        return 4..x;
    }

    return 0;
}

// '4. .'
function f2(a) {
    switch(a) {
    case "member":
        return 4. .x;
    case "arrayget":
        return 4. ["x"];
    case "constr":
        return 4.();
    case "funccall":
        return 4. .f();
    case "parenfunccall":
        return (4. .x)();
    case "assignment":
        return 4. .x = 33;
    case "assignment2":
        return 4. .x >>>= 1;
    case "prefix":
        return ++4. .x;
    case "postfix":
        return 4. .x++;
    case "delete":
        delete 4. .x;
        return 4. .x;
    }

    return 0;
}

// '4e20'
function f2(a) {
    switch(a) {
    case "member":
        return 4e20.x;
    case "arrayget":
        return 4e20["x"];
    case "constr":
        return 4e20();
    case "funccall":
        return 4e20.f();
    case "parenfunccall":
        return (4e20.x)();
    case "assignment":
        return 4e20.x = 33;
    case "assignment2":
        return 4e20.x >>>= 1;
    case "prefix":
        return ++4e20.x;
    case "postfix":
        return 4e20.x++;
    case "delete":
        delete 4e20.x;
        return 4e20.x;
    }

    return 0;
}

// '4.1e-20'
function f3(a) {
    switch(a) {
    case "member":
        return 4.1e-20.x;
    case "arrayget":
        return 4.1e-20["x"];
    case "constr":
        return 4.1e-20();
    case "funccall":
        return 4.1e-20.f();
    case "parenfunccall":
        return (4.1e-20.x)();
    case "assignment":
        return 4.1e-20.x = 33;
    case "assignment2":
        return 4.1e-20.x >>>= 1;
    case "prefix":
        return ++4.1e-20.x;
    case "postfix":
        return 4.1e-20.x++;
   case "delete":
        delete 4.1e-20.x;
        return 4.1e-20.x;
    }

    return 0;
}

// '4'
function f4(a) {
    switch(a) {
    case "member":
        return 4 .x;
    case "arrayget":
        return 4["x"];
    case "constr":
        return 4();
    case "funccall":
        return 4 .f();
    case "parenfunccall":
        return (4 .x)();
    case "assignment":
        return 4 .x = 33;
    case "assignment2":
        return 4 .x >>>= 1;
    case "prefix":
        return ++4 .x;
    case "postfix":
        return 4 .x++;
    case "delete":
        delete 4 .x;
        return 4 .x;

    }

    return 0;
}

// '(4)'
function f5(a) {
    switch(a) {
    case "member":
        return (4).x;
    case "arrayget":
        return (4)["x"];
    case "constr":
        return (4)();
    case "funccall":
        return (4).f();
    case "parenfunccall":
        return ((4).x)();
    case "assignment":
        return (4).x = 33;
    case "assignment2":
        return (4).x >>>= 1;
    case "prefix":
        return ++(4).x;
    case "postfix":
        return (4).x++;
    case "delete":
        delete (4).x;
        return (4).x;
    }

    return 0;
}
unevalf = function(x) { return '(' + x.toString() + ')'; };

function testToString(fn) 
{
    shouldBe("unevalf(eval(unevalf(" + fn + ")))", "unevalf(" + fn + ")");
}

for(var i = 1; i < 6; ++i)
    testToString("f" + i);

var successfullyParsed = true;
