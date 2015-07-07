description('Test the disabled property on a style element.');

var styleElement = document.getElementById('s');
var console = document.getElementById('console');
var testElement = document.createElement('div');
testElement.innerText = "Test element";
testElement.className = 'test';
document.body.appendChild(testElement);

shouldBeFalse('styleElement.disabled');
shouldBe('window.getComputedStyle(testElement).color', '"rgb(0, 128, 0)"');

styleElement.disabled = true
shouldBeTrue('styleElement.disabled');
shouldBe('window.getComputedStyle(testElement).color', '"rgb(255, 0, 0)"');

// Test reflection in the sheet.
shouldBeTrue('styleElement.sheet.disabled');
styleElement.sheet.disabled = false
shouldBeFalse('styleElement.sheet.disabled');
shouldBeFalse('styleElement.disabled');
shouldBe('window.getComputedStyle(testElement).color', '"rgb(0, 128, 0)"');

// Test disconnected element
var newStyleElement = document.createElement('style');
shouldBeFalse('newStyleElement.disabled');
newStyleElement.disabled = true
shouldBeFalse('newStyleElement.disabled');

// Test non-CSS element
var otherStyle = document.getElementById('non-css');
shouldBeFalse('otherStyle.disabled');
otherStyle.disabled = true
shouldBeFalse('otherStyle.disabled');


document.body.removeChild(testElement);
