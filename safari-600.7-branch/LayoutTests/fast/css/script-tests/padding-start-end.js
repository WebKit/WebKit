description('Tests padding-start and padding-end');

function test(dir, prop, value, queryProp)
{
    var div = document.createElement('div');
    div.setAttribute('style', 'width:100px;' + prop + ':' + value);
    div.dir = dir;
    document.body.appendChild(div);

    var result = getComputedStyle(div).getPropertyValue(queryProp);
    document.body.removeChild(div);
    return result;
}

function testWidth(dir, prop, value)
{
    var div = document.createElement('div');
    div.setAttribute('style', 'width:100px;' + prop + ':' + value);
    div.dir = dir;
    document.body.appendChild(div);

    var result = div.offsetWidth;
    document.body.removeChild(div);
    return result;
}

shouldBe('testWidth("ltr", "-webkit-padding-start", "10px")', '110');
shouldBe('testWidth("ltr", "-webkit-padding-end", "20px")', '120');
shouldBeEqualToString('test("ltr", "-webkit-padding-start", "10px", "padding-left")', '10px');
shouldBeEqualToString('test("ltr", "-webkit-padding-end", "20px", "padding-right")', '20px');
shouldBeEqualToString('test("ltr", "padding-left", "10px", "-webkit-padding-start")', '10px');
shouldBeEqualToString('test("ltr", "padding-right", "20px", "-webkit-padding-end")', '20px');

shouldBe('testWidth("rtl", "-webkit-padding-start", "10px")', '110');
shouldBe('testWidth("rtl", "-webkit-padding-end", "20px")', '120');
shouldBeEqualToString('test("rtl", "-webkit-padding-start", "10px", "padding-right")', '10px');
shouldBeEqualToString('test("rtl", "-webkit-padding-end", "20px", "padding-left")', '20px');
shouldBeEqualToString('test("rtl", "padding-right", "10px", "-webkit-padding-start")', '10px');
shouldBeEqualToString('test("rtl", "padding-left", "20px", "-webkit-padding-end")', '20px');
