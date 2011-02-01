description('Tests that computed style is not affected by the zoom value');

function testProperty(data)
{
    var prop = data[0];
    if (data.length == 1)
        data.push('5px');

    for (var i = 1; i < data.length; i++) {
        testPropertyValue(prop, data[i]);
    }
}

function testPropertyValue(prop, value)
{
    var el = document.createElement('div');
    el.style.cssText = 'position: absolute; width: 100px; height: 100px;' +
                       'overflow: hidden; border: 1px solid red;' +
                       'outline: 1px solid blue;-webkit-column-rule: 1px solid red';
    el.style.setProperty(prop, value, '');

    document.body.style.zoom = '';
    document.body.appendChild(el);

    var value1 = getComputedStyle(el, null).getPropertyValue(prop);
    document.body.style.zoom = 2;
    var value2 = getComputedStyle(el, null).getPropertyValue(prop);

    document.body.removeChild(el);
    document.body.style.zoom = '';

    if (typeof value1 === 'string' && value1 === value2)
        testPassed(prop + ', value: "' + value1 + '"');
    else
        testFailed(prop + ', value1: "' + value1 + '", value2: "' + value2 + '"');
}

var testData = [
    ['-webkit-border-horizontal-spacing'],
    ['-webkit-border-vertical-spacing'],
    ['-webkit-box-reflect', 'below 5px -webkit-gradient(linear, left top, left bottom, from(transparent), to(white))'],
    ['-webkit-box-shadow', '5px 5px 5px 5px red'],
    ['-webkit-column-rule-width', '5px'],
    ['-webkit-perspective-origin', '5px 5px'],
    ['-webkit-text-stroke-width'],
    ['-webkit-transform', 'translate(5px, 5px)', 'translate3d(5px, 5px, 5px)'],
    ['-webkit-transform-origin', '5px 5px'],
    ['border-bottom-left-radius'],
    ['border-bottom-right-radius'],
    ['border-bottom-width'],
    ['border-left-width'],
    ['border-right-width'],
    ['border-spacing'],
    ['border-top-left-radius'],
    ['border-top-right-radius'],
    ['border-top-width'],
    ['bottom'],
    ['clip', 'rect(5px 95px 95px 5px)'],
    ['font-size', '5px', 'medium'],
    ['height'],
    ['left'],
    ['letter-spacing'],
    ['line-height'],
    ['margin-bottom'],
    ['margin-left'],
    ['margin-right'],
    ['margin-top'],
    ['outline-width'],
    ['padding-bottom'],
    ['padding-left'],
    ['padding-right'],
    ['padding-top'],
    ['right'],
    ['text-shadow', '5px 5px 5px red'],
    ['top'],
    ['width'],
    ['word-spacing'],
];

testData.forEach(testProperty);

successfullyParsed = true;

