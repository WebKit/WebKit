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
    if (!window.internals)
        return null;
    var sel = window.getSelection();
    var selElement = document.getElementById(id);
    sel.setPosition(selElement, 0);
    return internals.findEditingDeleteButton();
}

function determineDeletionUIExistence(id)
{
    var deleteButton = deletionUIDeleteButtonForElement(id);
    debug(id + (deleteButton ? " HAS " : " has NO ") + "deletion UI");
}
