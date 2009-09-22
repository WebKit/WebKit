description(
"This tests that for/in statements don't report properties that are in both an object and its prototype more than once."
);


function constr() {
    this.xxx = "foo";
    this.yyy = "bar";
}

constr.prototype = { xxx: "baz", yyy: "quux" };

var obj = new constr();

var i = 0;
for (var prop in obj) {
    ++i;
}

shouldBe("i", "2");

var successfullyParsed = true;
