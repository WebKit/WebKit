function init() {
    // FIXME: chrome/blink is ridiculously bad at this, it would be interesting to find out why.
    var overlay = document.createElement('div');
    overlay.style.width = "100%";
    overlay.style.height = "100%";
    overlay.style.backgroundColor = "rgba(0,0,0,0.05)";
    overlay.style.margin = "0";
    overlay.style.position = "fixed";
    overlay.style.top = "0";
    overlay.style.left = "0";
    overlay.style.webkitTransition = "opacity 0.5s";
    overlay.style.transition = "opacity 0.5s";
    
    var overlayText = document.createElement('div');
    overlayText.textContent = "Click to start";
    overlayText.style.width = "80%";
    overlayText.style.backgroundColor = "rgba(0, 0, 0, 0.75)";
    overlayText.style.boxSizing = "border-box";
    overlayText.style.padding = "10px";
    overlayText.style.borderRadius = "25px";
    overlayText.style.textAlign = "center";
    overlayText.style.fontSize = "18px";
    overlayText.style.color = "white";
    overlayText.style.position = "absolute";
    overlayText.style.top = "45%";
    overlayText.style.left = "10%";
    overlay.appendChild(overlayText);
    document.body.insertBefore(overlay, document.body.firstChild);
    document.documentElement.style.webkitUserSelect = "none";

    function prepareForClick() {
        document.querySelector('svg').classList.remove("animated");
        overlay.style.opacity = "1";
        document.documentElement.style.cursor = "hand";
        document.documentElement.addEventListener("click", animateOnClick);
    }

    function animateOnClick() {
        document.documentElement.style.cursor = "auto";
        overlay.style.opacity = "0";
        document.querySelector('svg').classList.add("animated");
    }
    
    document.getElementById("WorkGear").addEventListener("webkitAnimationEnd", prepareForClick);
    prepareForClick();
}
init();