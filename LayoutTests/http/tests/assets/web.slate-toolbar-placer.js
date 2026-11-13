// Mimics discord.com: positions #slate-toolbar without clamping it to the viewport.
function createSlateToolbar({ parent = document.body, width = 200, left = null, right = null, style = "" })
{
    document.getElementById("slate-toolbar")?.remove();
    const toolbar = document.createElement("div");
    toolbar.id = "slate-toolbar";
    toolbar.style.cssText = `position: absolute; top: 20px; height: 40px; background: blue; width: ${width}px; ${style}`;
    if (left !== null)
        toolbar.style.left = left + "px";
    if (right !== null)
        toolbar.style.right = right + "px";
    parent.appendChild(toolbar);
    return toolbar;
}
