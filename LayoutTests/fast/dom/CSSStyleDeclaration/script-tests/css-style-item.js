description("This tests indexing outside the range of the style object.");

var element = document.createElement('div');
element.style.color = 'red';
document.documentElement.appendChild(element);
var style = element.style;

shouldEvaluateTo('style.length', 1);
shouldBeEqualToString('style[0]', 'color');
shouldBeEqualToString('style[1]', '');
shouldBeUndefined('style[-1]')

document.documentElement.removeChild(element);

successfullyParsed = true;
