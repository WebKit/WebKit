description("This tests the constructor returning masquerades as undefined return masquerades.");

function Constructor() {
    return document.all;
}
var result = new Constructor();
shouldBe("result", "document.all");
