function checkHasShortFormat()
{
    var container = document.createElement("span");
    container.innerHTML = "<input type=time id=step60 step=60><input type=time id=step1 step=1>";
    document.body.appendChild(container);
    var hasShortFormat = document.getElementById("step60").offsetWidth != document.getElementById("step1").offsetWidth;
    container.parentElement.removeChild(container);
    return hasShortFormat;
}

var hasShortFormat = checkHasShortFormat();

function setUpTimeType(host)
{
    var step = parseFloat(host.getAttribute("step"));
    if (isNaN(step) || step <= 0)
        step = 60;

    var value = host.getAttribute("value");
    if (!value)
        value = "--:--:--.--- --";

    var element = document.createElement("span");
    element.className = "edit";
    host.appendChild(element);

    var hourField = document.createElement("span");
    hourField.className = "hourField";
    hourField.innerHTML = value.substring(0, 2);
    element.appendChild(hourField);

    var minuteField = document.createElement("span");
    minuteField.className = "minuteField";
    minuteField.innerHTML = value.substring(3, 5);
    if (!(step % 3600)) {
        minuteField.style.color = "grayText";
        if (minuteField.innerHTML == "--")
            minuteField.innerHTML = "00";
    }
    element.appendChild(document.createTextNode(":"));
    element.appendChild(minuteField);

    var needSecondField = (step % 60) != 0;
    if (!hasShortFormat || needSecondField) {
        var secondField = document.createElement("span");
        secondField.className = "secondField";
        secondField.innerHTML = value.substring(6, 8);
        if (!needSecondField) {
            secondField.style.color = "grayText";
            if (secondField.innerHTML == "--")
                secondField.innerHTML = "00";
        }
        element.appendChild(document.createTextNode(":"));
        element.appendChild(secondField);

        if (step % 1) {
            var millisecondField = document.createElement("span");
            millisecondField.className = "millisecondField";
            millisecondField.innerHTML = value.substring(9, 12);
            element.appendChild(document.createTextNode("."));
            element.appendChild(millisecondField);
        }
    }

    var ampmField = document.createElement("span");
    ampmField.className = "ampmField";
    ampmField.innerHTML = value[0] == '-' ? "--" : "PM";
    element.appendChild(document.createTextNode(" "));
    element.appendChild(ampmField);

    var spinButton = document.createElement("span");
    spinButton.className = "spinButton";
    element.appendChild(spinButton);
    if (host.hasAttribute("disabled") || host.hasAttribute("readonly")) {
      spinButton.style.color = "GrayText";
    }

    if (host.className == "after")
        host.appendChild(document.createTextNode("[after]"));

    host.className = "host";
}

function walker(node)
{
    if (node.nodeType != 1)
        return;

    if (node.tagName != "INPUT" && node.getAttribute("type") == "time") {
        setUpTimeType(node);
        return;
    }

    var childNodes = node.childNodes;
    for (var i = 0; i < childNodes.length; ++i) {
        walker(childNodes[i]);
    }
}
walker(document.body);
