description("This tests checks parsing of the '-webkit-overflow-scrolling' property.");

function test(declaration, property)
{
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    document.body.appendChild(div);
    
    var result = div.style.getPropertyValue(property);
    document.body.removeChild(div);
    return result;
}

shouldBe('test("-webkit-overflow-scrolling: auto", "-webkit-overflow-scrolling")', '"auto"');
shouldBe('test("-webkit-overflow-scrolling: banana", "-webkit-overflow-scrolling")', 'null');
shouldBe('test("-webkit-overflow-scrolling: touch", "-webkit-overflow-scrolling")', '"touch"');

var successfullyParsed = true;
