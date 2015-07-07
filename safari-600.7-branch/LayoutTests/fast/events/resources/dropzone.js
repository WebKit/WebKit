function drop(e)
{
    printDropEvent(e);
    cancelDrag(e);
}
    
function cancelDrag(e)
{
    e.preventDefault();
}
