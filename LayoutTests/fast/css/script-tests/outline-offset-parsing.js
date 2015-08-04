description("This tests checks parsing of the 'outline-offset' property.");

function test(declaration, property)
{
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    document.body.appendChild(div);

    var result = div.style.getPropertyValue(property);
    document.body.removeChild(div);
    return result;
}

shouldBe('test("outline-offset: 1px", "outline-offset")', '"1px"');
shouldBe('test("outline-offset: 1mm", "outline-offset")', '"1mm"');
shouldBeEqualToString('test("outline-offset: 1%", "outline-offset")', '');
