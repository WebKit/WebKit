// To double check location of touches or other x,y coordinates
function debugDot(x, y, element = document.body)
{
    var dotSize = 10;
    var dot = document.getElementById("debugDot");
    if (!dot) {
        dot = document.createElement("div");
        dot.id = "debugDot";
    }
    
    dot.style.pointerEvents = "none";
    
    dot.style.position = "absolute";
    dot.style.height = dotSize + "px";
    dot.style.width = dotSize + "px";
    dot.style.backgroundColor = "DeepPink";
    dot.style.borderRadius = "100%";
    dot.style.left = (x - dotSize/2)+"px";
    dot.style.top = (y - dotSize/2)+"px";
    
    element.appendChild(dot);
}

function removeDebugDot()
{
    var dot = document.getElementById("debugDot");
    if (dot.parentNode)
        dot.parentNode.removeChild(dot);
}

// To double check location of word boxes or other x,y coordinates
function debugRect(x, y, width, height, element = document.body)
{
    var rect = document.getElementById("debugRect");
    if (!rect) {
        rect = document.createElement("div");
        rect.id = "debugRect";
    }
    
    rect.style.pointerEvents = "none";
    
    rect.style.position = "absolute";
    rect.style.height = height + "px";
    rect.style.width = width + "px";
    rect.style.backgroundColor = "rgba(255,0,255,0.5)";
    rect.style.top = y + "px";
    rect.style.left = x + "px";
    
    element.appendChild(rect);
}

function removeDebugRect()
{
    var rect = document.getElementById("debugRect");
    if (rect.parentNode)
        rect.parentNode.removeChild(rect);
}

