<?php
header("Cache-control: no-cache, max-age=0");
header("Last-Modified: Thu, 01 Jan 1970 00:00:00 GMT");
header('Content-Type: text/html');

print "<script>\n";
print "var randomNumber = " . rand() . ";\n";
?>

opener.postMessage(randomNumber, '*');

// Our opener will tell us to perform various loads.
window.addEventListener('message', function(event) {

    // Go forward and back.
    if (event.data === 'go-forward-and-back') {
        window.location = 'history-back.html';
        return;
    }

    // Reload.
    if (event.data === 'reload') {
        window.location.reload();
        return;
    }

    // Normal navigation, next.
    if (event.data === 'next') {
        window.location = 'no-cache-main-resource-next.php';
        return;
    }

}, false);
</script>
