description('Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=50335">https://bugs.webkit.org/show_bug.cgi?id=50335</a>');

var input = document.createElement('input');

input.setAttribute('incremental', '')
shouldBeTrue('input.incremental');

input.removeAttribute('incremental');
shouldBeFalse('input.incremental');

input.incremental = true;
shouldBeTrue('input.hasAttribute("incremental")');

input.incremental = false;
shouldBeFalse('input.hasAttribute("incremental")');
