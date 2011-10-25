description('Tests border-start and border-end');

function test(dir, css, queryProp)
{
    var div = document.createElement('div');
    div.setAttribute('style', css);
    div.dir = dir;
    document.body.appendChild(div);

    var result = getComputedStyle(div).getPropertyValue(queryProp);
    document.body.removeChild(div);
    return result;
}

function testSame(dir, prop, altProp, value)
{
    shouldBeEqualToString('test("' + dir + '", "' + prop + ': ' + value + '", "' + altProp + '")', value);
    shouldBeEqualToString('test("' + dir + '", "' + altProp + ': ' + value + '", "' + prop + '")', value);
}

function testWidth(dir, style)
{
    var div = document.createElement('div');
    div.setAttribute('style', 'width:100px;' + style);
    div.dir = dir;
    document.body.appendChild(div);

    var result = div.offsetWidth;
    document.body.removeChild(div);
    return result;
}

shouldBe('testWidth("ltr", "-webkit-border-start-width: 10px; -webkit-border-start-style: solid")', '110');
shouldBe('testWidth("ltr", "-webkit-border-end-width: 20px; -webkit-border-end-style:  solid")', '120');
shouldBe('testWidth("rtl", "-webkit-border-start-width: 10px; -webkit-border-start-style:  solid")', '110');
shouldBe('testWidth("rtl", "-webkit-border-end-width: 20px; -webkit-border-end-style:  solid")', '120');

testSame('ltr', '-webkit-border-start-color', 'border-left-color', 'rgb(255, 0, 0)');
testSame('ltr', '-webkit-border-end-color', 'border-right-color', 'rgb(255, 0, 0)');
testSame('rtl', '-webkit-border-start-color', 'border-right-color', 'rgb(255, 0, 0)');
testSame('rtl', '-webkit-border-end-color', 'border-left-color', 'rgb(255, 0, 0)');

testSame('ltr', '-webkit-border-start-style', 'border-left-style', 'dotted');
testSame('ltr', '-webkit-border-end-style', 'border-right-style', 'dotted');
testSame('rtl', '-webkit-border-start-style', 'border-right-style', 'dotted');
testSame('rtl', '-webkit-border-end-style', 'border-left-style', 'dotted');

shouldBeEqualToString('test("ltr", "-webkit-border-start-width: 10px; -webkit-border-start-style: solid", "border-left-width")', '10px');
shouldBeEqualToString('test("ltr", "-webkit-border-end-width: 10px; -webkit-border-end-style: solid", "border-right-width")', '10px');
shouldBeEqualToString('test("rtl", "-webkit-border-start-width: 10px; -webkit-border-start-style: solid", "border-right-width")', '10px');
shouldBeEqualToString('test("rtl", "-webkit-border-end-width: 10px; -webkit-border-end-style: solid", "border-left-width")', '10px');

shouldBeEqualToString('test("ltr", "border-left: 10px solid", "-webkit-border-start-width")', '10px');
shouldBeEqualToString('test("ltr", "border-right: 10px solid", "-webkit-border-end-width")', '10px');
shouldBeEqualToString('test("rtl", "border-left: 10px solid", "-webkit-border-end-width")', '10px');
shouldBeEqualToString('test("rtl", "border-right: 10px solid", "-webkit-border-start-width")', '10px');

shouldBeEqualToString('test("ltr", "-webkit-border-start: 10px solid red", "border-left-color")', 'rgb(255, 0, 0)');
shouldBeEqualToString('test("ltr", "-webkit-border-start: 10px solid red", "border-left-style")', 'solid');
shouldBeEqualToString('test("ltr", "-webkit-border-start: 10px solid red", "border-left-width")', '10px');

shouldBeEqualToString('test("rtl", "-webkit-border-start: 10px solid red", "border-right-color")', 'rgb(255, 0, 0)');
shouldBeEqualToString('test("rtl", "-webkit-border-start: 10px solid red", "border-right-style")', 'solid');
shouldBeEqualToString('test("rtl", "-webkit-border-start: 10px solid red", "border-right-width")', '10px');

shouldBeEqualToString('test("ltr", "-webkit-border-end: 10px solid red", "border-right-color")', 'rgb(255, 0, 0)');
shouldBeEqualToString('test("ltr", "-webkit-border-end: 10px solid red", "border-right-style")', 'solid');
shouldBeEqualToString('test("ltr", "-webkit-border-end: 10px solid red", "border-right-width")', '10px');

shouldBeEqualToString('test("rtl", "-webkit-border-end: 10px solid red", "border-left-color")', 'rgb(255, 0, 0)');
shouldBeEqualToString('test("rtl", "-webkit-border-end: 10px solid red", "border-left-style")', 'solid');
shouldBeEqualToString('test("rtl", "-webkit-border-end: 10px solid red", "border-left-width")', '10px');
