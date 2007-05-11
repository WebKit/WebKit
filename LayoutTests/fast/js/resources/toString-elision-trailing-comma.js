description(
"This test checks that toString() round-trip on a function that has a array with elision does not remove a comma."
);

function f1() {
    return [1,,,];
}

function f2() {
    return [1,,,,,];
}

unevalf = function(x) { return '(' + x.toString() + ')'; }

try {
    uf1 = unevalf(f1);
    ueuf1 = unevalf(eval(unevalf(f1)));

    uf2 = unevalf(f2);
    ueuf2 = unevalf(eval(unevalf(f2)));
} catch(e) {
}

shouldBe("ueuf1", "uf1");
shouldBe("ueuf2", "uf2");

var successfullyParsed = true;
