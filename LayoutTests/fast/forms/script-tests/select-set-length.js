description('Tests that setting the .length of an HTMLSelectElement correctly creates and destroys options.');

var sel = document.createElement('select');
document.body.appendChild(sel);

shouldBe('sel.length', '0');

sel.length = 200;
shouldBe('sel.length', '200');

sel.length = 100;
shouldBe('sel.length', '100');

sel.length = 180;
shouldBe('sel.length', '180');
