function runTest(window)
{
    function eventHandler(event)
    {
        console.log(event.type + "event dispatched (isTrusted: " + event.isTrusted + ").");
    }

    window.addEventListener("keydown", eventHandler, true);
    window.addEventListener("keypress", eventHandler, true);
    window.addEventListener("keyup", eventHandler, true);
    window.addEventListener("compositionstart", eventHandler, true);
    window.addEventListener("compositionupdate", eventHandler, true);
    window.addEventListener("compositionend", eventHandler, true);
    window.addEventListener("textInput", eventHandler, true);
    window.addEventListener("beforeinput", eventHandler, true);
    window.addEventListener("input", eventHandler, true);

    var document = window.document;

    console.log("Dispatching untrusted keypress event.");
    var keyPressEvent = new KeyboardEvent("keypress");
    document.body.dispatchEvent(keyPressEvent);

    var textInput = document.createElement("input");
    textInput.type = "text";
    document.body.appendChild(textInput);

    console.log("Pressing tab.");
    eventSender.keyDown("\t");
    console.log("Active element after pressing tab: " + document.activeElement + ".");

    console.log("Pressing \"a\".");
    eventSender.keyDown("a");

    console.log("Setting marked text to \"b\".");
    textInputController.setMarkedText("b", 0, 1);

    console.log("Inserting text \"c\".");
    textInputController.insertText("c");

    console.log("Input element value after text input events: \"" + textInput.value + "\".");
}

function waitForProvisionalNavigation(completionHandler)
{
    // This exploits the fact that XHRs are cancelled when a location change begins.
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        if (this.readyState === this.DONE)
            window.setTimeout(completionHandler, 0);
    };
    xhr.open("GET", "resources/never-respond.php");
    xhr.send();

    window.location = "resources/never-respond.php";
}
