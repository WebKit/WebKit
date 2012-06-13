if (window.testRunner)
    testRunner.dumpAsText();

function debug(msg)
{
    var span = document.createElement("span");
    document.getElementById("console").appendChild(span); // insert it first so XHTML knows the namespace
    span.innerHTML = msg + '<br />';
}

function deletionUIDeleteButtonForElement(id)
{
    var sel = window.getSelection();
    var selElement = document.getElementById(id);
    sel.setPosition(selElement, 0);
    var deleteButton = document.getElementById("WebKit-Editing-Delete-Button");
    return deleteButton;
}

function determineDeletionUIExistence(id)
{
    var deleteButton = deletionUIDeleteButtonForElement(id);
    debug(id + (deleteButton ? " HAS " : " has NO ") + "deletion UI");
}
