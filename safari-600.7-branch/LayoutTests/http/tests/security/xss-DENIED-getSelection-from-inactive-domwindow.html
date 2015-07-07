<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

window.onload = function()
{
    frame = document.body.appendChild(document.createElement("iframe"));
    wnd = frame.contentWindow;
    func = wnd.Function;

    wnd.location = "about:blank";
    frame.onload = function() {
        selection = func("return getSelection()")();

        wnd.location = "http://localhost:8000/security/resources/innocent-victim.html";
        frame.onload = function() {
            frame.onload = null;

            try {
                selection.baseNode.constructor.constructor.constructor("alert(document.body.innerHTML)")()
            } catch(ex) {
            }

            if (window.testRunner)
                testRunner.notifyDone();
        }
    }
}
</script>
This tests passes if it doesn't alert the contents of innocent-victim.html.
