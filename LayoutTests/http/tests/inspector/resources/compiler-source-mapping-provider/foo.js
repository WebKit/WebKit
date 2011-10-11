function ClickHandler()
{
}

ClickHandler.prototype.handle = function(event)
{
    var element = document.createElement('div');
    element.textContent = event.timeStamp;
    document.body.appendChild(element);
}
