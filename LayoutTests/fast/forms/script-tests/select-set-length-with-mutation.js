description('Tests setting the .length of an HTMLSelectElement with mutation listeners registered.');

var sel = document.createElement('select');
document.body.appendChild(sel);

sel.addEventListener('DOMNodeRemoved', function() {}, false);
sel.addEventListener('DOMNodeInserted', function() {}, false);

sel.length = 200;
shouldBe('sel.length', '200');

sel.length = 100;
shouldBe('sel.length', '100');

sel.length = 180;
shouldBe('sel.length', '180');
