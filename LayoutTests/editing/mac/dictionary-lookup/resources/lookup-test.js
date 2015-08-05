function runTest() {
    if (!window.testRunner || !internals) {
        document.body.innerHTML += "<br/>ERROR: This test cannot be run interactively.";
        return;
    }

    testRunner.dumpAsText();

    var targets = document.getElementsByClassName("lookupInCenter");
    Array.prototype.forEach.call(targets, function(target) {
        var spanRects = target.getClientRects();

        if (spanRects.count > 1) {
            document.body.innerHTML += "<br/>ERROR: More than one rect for hit word."
            return;
        }

        var rect = spanRects[0];
        var x = rect.left + rect.width / 2;
        var y = rect.top + rect.height / 2;

        var lookupRange = internals.rangeForDictionaryLookupAtLocation(x, y);
        
        document.body.innerHTML += "<br/>Lookup string for " + target.dataset["name"] + ": '" + lookupRange + "'.";
    });
}