const baseHeight = 200;
let curHeight = baseHeight;

function toggleHeight(element, amount)
{
    const oldHeight = curHeight;
    curHeight = (curHeight === baseHeight) ? curHeight + amount : baseHeight;
    element.style.height = `${curHeight}px`;
    return curHeight - oldHeight;
}
