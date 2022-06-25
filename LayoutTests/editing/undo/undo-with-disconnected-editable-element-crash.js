if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    console.log('This test passes if it does not crash.');
}
window.parent.onwebkitanimationiteration = () => {
    try { inputElement.select(); } catch(e) { }
    document.execCommand("selectAll",false,null);
    try { inputElement.type = "password"; } catch (e) { }

    var olElement = document.createElement("ol");
    olElement.addEventListener("DOMSubtreeModified", () => {
        try { inputElement.selectionDirection = "forward"; } catch (e) { }
        document.execCommand("fontSize",false,98);
        document.execCommand("undo",false,null);
        document.execCommand("redo",false,null);
        testRunner.notifyDone();
    });
    olElement.compact = true;

    try { inputElement.outerHTML = "B" } catch (e) { };
    if (window.GCController)
        GCController.collect();
};
