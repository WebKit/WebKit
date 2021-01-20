<!doctype html>
<script>

var from_http = <?php
echo json_encode($_COOKIE);
?>;

window.addEventListener("message", e => {
    e.source.postMessage({
        'http': from_http,
        'document': document.cookie
    }, "*");
});
</script>
