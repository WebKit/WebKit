description("This tests indexing outside the range of the computed style object.");

var element = document.createElement('div');
element.style.color = 'rgb(120, 120, 120)';
document.documentElement.appendChild(element);
var computedStyle = window.getComputedStyle(element, null);

shouldBeEqualToString('typeof computedStyle.length', 'number');
shouldBeEqualToString('computedStyle[computedStyle.length]', '');
shouldBeUndefined('computedStyle[-1]')

document.documentElement.removeChild(element);

successfullyParsed = true;
