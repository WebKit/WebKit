var style = document.createElement('style');
style.appendChild(document.createTextNode("input { color: green; }"));
style.appendChild(document.createTextNode("input:checked { color: red; }"));
document.documentElement.firstChild.appendChild(style);

var input1 = document.createElement('input');
input1.type = 'radio';
document.body.appendChild(input1);
input1.checked = true;
input1.type = "text";

var view = document.defaultView;
shouldBeEqualToString("view.getComputedStyle(input1, '').getPropertyValue('color')", "rgb(0, 128, 0)");
shouldBeTrue("input1.checked");

// cleanup
document.body.removeChild(input1);

var successfullyParsed = true;
