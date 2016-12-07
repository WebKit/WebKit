description("This tests checks parsing of the '-webkit-transform-origin' property \
    and in particular that specifying invalid z values discards the property.");

function test(declaration, property)
{
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    document.body.appendChild(div);
    
    var result = div.style.getPropertyValue(property);
    document.body.removeChild(div);
    return result;
}

shouldBeEqualToString('test("-webkit-transform-origin: 10% 10% 10%", "-webkit-transform-origin")', '');
shouldBe('test("-webkit-transform-origin: 10% 10% 10px", "-webkit-transform-origin")', '"10% 10% 10px"');
shouldBeEqualToString('test("-webkit-transform-origin: 10px 10px 10%", "-webkit-transform-origin")', '');
shouldBe('test("-webkit-transform-origin: 10px 10px 10px", "-webkit-transform-origin")', '"10px 10px 10px"');
shouldBeEqualToString('test("-webkit-transform-origin: left top 10%", "-webkit-transform-origin")', '');
shouldBe('test("-webkit-transform-origin: left top 10px", "-webkit-transform-origin")', '"left top 10px"');
shouldBeEqualToString('test("-webkit-transform-origin: left top left", "-webkit-transform-origin")', '');
