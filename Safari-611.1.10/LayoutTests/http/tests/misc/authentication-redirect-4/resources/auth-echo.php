<script>
    document.write(document.location);
    document.write(" loaded with HTTP authentication username '<? echo $_SERVER["PHP_AUTH_USER"] ?>' and password '<? echo $_SERVER["PHP_AUTH_PW"] ?>'");
    if (window.testRunner)
        window.testRunner.notifyDone();
</script>
