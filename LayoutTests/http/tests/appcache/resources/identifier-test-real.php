<?php
    header('X-Frame-Options: SAMEORIGIN');
?>

<html manifest="identifier-test.manifest">
<script>

var sentMessage = false;
function cached()
{
    if (sentMessage)
        return;

    sentMessage = true;
    window.opener.postMessage("Nice", "*");
}

applicationCache.addEventListener('cached', cached, false);
applicationCache.addEventListener('noupdate', cached, false);

</script>
</html>
