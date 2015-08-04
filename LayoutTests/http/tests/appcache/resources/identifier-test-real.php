<?php
    header('X-Frame-Options: SAMEORIGIN');
?>

<html manifest="identifier-test.manifest">
<script>

function cached()
{
    window.opener.postMessage("Nice", "*");
}

applicationCache.addEventListener('cached', cached, false);
applicationCache.addEventListener('noupdate', cached, false);

</script>
</html>
