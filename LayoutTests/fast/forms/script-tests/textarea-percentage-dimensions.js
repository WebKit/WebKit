
function heightChanged(style)
{
    window.targetContainer.innerHTML = "<textarea id='target' style='" + style + "'></textarea>";
    var target = document.getElementById("target");

    var heightBeforeInsert = target.clientHeight;

    target.focus();
    document.execCommand("InsertText", false, "Test");

    var heightAfterInsert = target.clientHeight;

    return heightBeforeInsert - heightAfterInsert;
}

window.targetContainer = document.createElement("div");
document.body.appendChild(window.targetContainer);

shouldBe('heightChanged("height:70%;")', "0");
shouldBe('heightChanged("margin-bottom: 20%;")', "0");
shouldBe('heightChanged("margin-top:20%;")', "0");
shouldBe('heightChanged("height:2000px; max-height:80%;")', "0");
shouldBe('heightChanged("height:100px; min-height:100%;")', "0");
shouldBe('heightChanged("padding-bottom: 20%;")', "0");
shouldBe('heightChanged("padding-top: 20%;")', "0");

var successfullyParsed = true;
