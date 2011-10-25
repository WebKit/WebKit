description('Tests that setting .length on an HTMLSelectElement works in the presence of mutation listeners that remove option elements.');

function gc() {
    if (window.GCController)
        return GCController.collect();

    for (var i=0; i<10000; ++i) {
        var s = new String("abc");
    }
}

function onRemove(e) {
    if (e.target.nextSibling != null) {
        // remove listener temporarily to avoid lots of nesting
        sel.removeEventListener('DOMNodeRemoved', onRemove, false);
        e.target.nextSibling.parentNode.removeChild(e.target.nextSibling);
        sel.addEventListener('DOMNodeRemoved', onRemove, false);
    }
    gc();
}

var sel = document.createElement('select');
document.body.appendChild(sel);

sel.addEventListener('DOMNodeRemoved', onRemove, false);
sel.addEventListener('DOMNodeInserted', function() {}, false);

sel.length = 200;
shouldBe('sel.length', '200');

sel.length = 100;
shouldBe('sel.length', '100');

sel.length = 180;
shouldBe('sel.length', '180');
