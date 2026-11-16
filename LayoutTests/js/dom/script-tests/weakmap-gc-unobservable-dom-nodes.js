var wmap = new WeakMap();

// Need to add several elements so the bug manifests.
for (var i = 0; i < 5; i++) {
    document.body.appendChild(document.createElement('p'));
    wmap.set(document.body.lastElementChild, 4);
}

gc();

Array.prototype.forEach.call(
    document.querySelectorAll('p'), function(el) {
	shouldBeDefined(wmap.get(el));
    });

