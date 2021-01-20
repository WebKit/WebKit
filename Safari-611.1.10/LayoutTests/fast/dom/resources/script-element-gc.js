function gc()
{
    if (window.GCController)
        return GCController.collect();

    for (var i = 0; i < 10000; i++) { // > force garbage collection (FF requires about 9K allocations before a collect)
        var s = new String("");
    }
}

function removeScriptElement() {
    var s = document.getElementById('theScript');
    s.parentNode.removeChild(s);
}

removeScriptElement();
gc();
