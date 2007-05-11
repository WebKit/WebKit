description(
"This test checks that toString() round-trip on a function that has a expression of form 4..x does not lose its meaning."
+ " The expression accesses the property 'x' from number '4'."
);

function f1() {
    return 4..x;
}

function f2() {
    return 4e20.x;
}

function f3() {
    return 4.0.x;
}

function f4() {
    return 4.[x];
}

function f5() {
    return 4.();
}

function f6() {
    return 4..f();
}




unevalf = function(x) { return '(' + x.toString() + ')'; }

try {
    uf1 = unevalf(f1);
    ueuf1 = unevalf(eval(unevalf(f1)));
}  catch(e) {};
try {
    uf2 = unevalf(f2);
    ueuf2 = unevalf(eval(unevalf(f2)));
} catch(e) {};
try {
    uf3 = unevalf(f3);
    ueuf3 = unevalf(eval(unevalf(f3)));
} catch(e) {};
try {
    uf4 = unevalf(f4);
    ueuf4 = unevalf(eval(unevalf(f4)));
} catch(e) {};
try {
    uf5 = unevalf(f5);
    ueuf5 = unevalf(eval(unevalf(f5)));
} catch(e) {};
try {
    uf6 = unevalf(f6);
    ueuf6 = unevalf(eval(unevalf(f6)));
} catch(e) {};

shouldBe("ueuf1", "uf1");
shouldBe("ueuf2", "uf2");
shouldBe("ueuf3", "uf3");
shouldBe("ueuf4", "uf4");
shouldBe("ueuf5", "uf5");
shouldBe("ueuf6", "uf6");


var successfullyParsed = true;
