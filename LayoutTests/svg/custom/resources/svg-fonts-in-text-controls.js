description('This tests that the width of textareas and inputs is correctly calculated based on the metrics of the SVG font.');

var styleElement = document.createElement('style');
// FIXME: Is there a better way to create a font-face from JS?
styleElement.innerText = '@font-face { font-family: "SVGraffiti"; src: url("resources/graffiti.svg#SVGraffiti") format(svg) }';
document.getElementsByTagName('head')[0].appendChild(styleElement);

var textarea = document.createElement('textarea');
textarea.style.fontFamily = 'SVGraffiti';
textarea.cols = 20;
document.body.appendChild(textarea);
shouldBe(String(textarea.offsetWidth), '141');

var input = document.createElement('input');
input.style.fontFamily = 'SVGraffiti';
input.size = 20;
document.body.appendChild(input);
shouldBe(String(input.offsetWidth), '140');

var successfullyParsed = true;
