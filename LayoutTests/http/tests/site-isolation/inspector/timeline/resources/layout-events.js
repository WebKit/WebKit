function forceLayout()
{
    let box = document.getElementById("box");
    box.style.width = (box.offsetWidth + 10) + "px";
    return box.offsetWidth;
}

// Largest Contentful Paint only counts content inside the viewport, so this goes first in the small iframe.
function prependLargeText()
{
    let element = document.createElement("div");
    element.id = "large-text";
    element.style.cssText = "font-size: 40px";
    element.textContent = "Largest text";
    document.body.prepend(element);
}
