description(
"Tests what happens when you make prototype chain accesses with impure GetOwnPropertySlot traps in the way."
);

var doc = document;

function f() {
    return doc.getElementsByTagName;
}

var expected = "\"function\"";
for (var i = 0; i < 40; ++i) {
    if (i == 35) {
        var img = new Image();
        img.name = "getElementsByTagName";
        document.body.appendChild(img);
        expected = "\"object\"";
    }
    shouldBe("typeof f()", expected);
}

