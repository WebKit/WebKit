description("This tests checks that the -webkit-sticky value for position parses correctly.");

function test(value)
{
    var div = document.createElement("div");
    div.setAttribute("style", value);
    document.body.appendChild(div);
    
    var result = div.style.getPropertyValue("position");
    document.body.removeChild(div);
    return result;
}

shouldBe('test("position: -webkit-sticky;")', '"-webkit-sticky"');
shouldBe('test("position: sticky;")', 'null');
