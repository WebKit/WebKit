description('Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=21248">https://bugs.webkit.org/show_bug.cgi?id=21248</a>');

var textarea = document.createElement('textarea');

shouldBeEqualToString('textarea.placeholder', '');

textarea.setAttribute('placeholder', 'p1')
shouldBeEqualToString('textarea.placeholder', 'p1');
shouldBeEqualToString('textarea.value', '');

textarea.placeholder = 'p2';
shouldBeEqualToString('textarea.getAttribute("placeholder")', 'p2');
shouldBeEqualToString('textarea.placeholder', 'p2');

var successfullyParsed = true;
