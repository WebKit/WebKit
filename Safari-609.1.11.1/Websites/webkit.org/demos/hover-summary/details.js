
function summaryClickListener(evt)
{
    if (this.parentNode.getAttribute("open") == null) {
        this.parentNode.setAttribute("open", "");
        this.parentNode.className = "xxx";
    } else {
        this.parentNode.removeAttribute("open");
        this.parentNode.className = "yyy";
    }
}


function detailsKeyPressListener(evt)
{
    if (this.getAttribute("open") == null) {
        this.setAttribute("open", "");
        this.className = "xxx";
    } else {
        this.removeAttribute("open");
        this.className = "yyy";
    }
}


window.onload = function() {
    var summaryElements = document.getElementsByTagName("summary");
    for (var i = 0; i < summaryElements.length; ++i) {
        summaryElements[i].parentNode.setAttribute("tabindex", 0);
        summaryElements[i].addEventListener("click", summaryClickListener, false);
        summaryElements[i].parentNode.addEventListener("keypress", detailsKeyPressListener, false);
    }
};