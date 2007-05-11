description(
"This test checks that toString() round-trip does not change meaning of functions that contain var declarations inside for -loop."
);

function f1() { for (var j = 1 in []) {}  }
var f2 = function () { for (var j = 1; j < 10; ++j) {}  }
var f3 = function () { for (j = 1;j < 10; ++j) {}  }

unevalf = function(x) { return '(' + x.toString() + ')'; }

try {
    uf1 = unevalf(f1);
    ueuf1 = unevalf(eval(unevalf(f1)));
}  catch(e) {};
try {
    uf2 = unevalf(f2);
    ueuf2 = unevalf(eval(unevalf(f2)));
} catch(e) {};

uf3 = unevalf(f3);

shouldBe("ueuf1", "uf1");
shouldBe("ueuf2", "uf2");
shouldBe("uf2 != uf3", "true");

var successfullyParsed = true;
