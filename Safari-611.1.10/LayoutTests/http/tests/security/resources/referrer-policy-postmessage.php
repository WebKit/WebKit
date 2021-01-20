<html>
<head>
<script>
function ownerWindow()
{
    var owner = window.parent;
    if (owner === this)
        owner = window.opener;
    return owner;
}

function log(message)
{
    ownerWindow().postMessage(message, "*");
}

function runTest()
{
    var referrerHeader = "<?php echo $_SERVER['HTTP_REFERER'] ?>";
    if (referrerHeader == "")
        log("HTTP Referer header is empty");
    else
        log("HTTP Referer header is " + referrerHeader);

    if (document.referrer == "")
        log("Referrer is empty");
    else
        log("Referrer is " + document.referrer);

    log("done");
}
</script>
</head>
<body onload="runTest()">
</body>
</html>
