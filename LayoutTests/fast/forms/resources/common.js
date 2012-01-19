function $(id) {
    return document.getElementById(id);
}

function getValidationMessageBubbleNode(host) {
    // FIXME: We should search for a pseudo ID.
    return internals.shadowRoot(host).lastChild;
}

function getAbsoluteRect(element) {
    var rect = element.getBoundingClientRect();
    rect.top += document.body.scrollTop;
    rect.bottom += document.body.scrollTop;
    rect.left += document.body.scrollLeft;
    rect.right += document.body.scrollLeft;
    return rect;
}

function searchCancelButtonPosition(element) {
    var pos = {};
    pos.x = element.offsetLeft + element.offsetWidth - 9;
    pos.y = element.offsetTop + element.offsetHeight / 2;
    return pos;
}

function mouseMoveToIndexInListbox(index, listboxId) {
    var listbox = document.getElementById(listboxId);
    var itemHeight = Math.floor(listbox.offsetHeight / listbox.size);
    var border = 1;
    var y = border + index * itemHeight;
    if (window.eventSender)
        eventSender.mouseMoveTo(listbox.offsetLeft + border, listbox.offsetTop + y - window.pageYOffset);
}
