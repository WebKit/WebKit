description(
"This test checks that toString() round-trip on a function that has do..while in JavaScript does not insert extra semicolon."
);

function f1() {
    do {} while(0);
}

function f2() {
    do {} while(0)
}

function f3() {
    do {} while(0)   ;
}

function f4() {
    do {} while(0)  /*empty*/ ;
}



if (typeof uneval == "undefined")
    uneval = function(x) { return '(' + x.toString()+ ')'; }


uf1 = uneval(f1);
ueuf1 = uneval(eval(uneval(f1)));

uf2 = uneval(f2);
ueuf2 = uneval(eval(uneval(f2)));

uf3 = uneval(f3);
ueuf3 = uneval(eval(uneval(f3)));

uf4 = uneval(f4);
ueuf4 = uneval(eval(uneval(f4)));



shouldBe("ueuf1", "uf1");
shouldBe("ueuf2", "uf2");
shouldBe("ueuf3", "uf3");
shouldBe("ueuf4", "uf4");

var successfullyParsed = true;
