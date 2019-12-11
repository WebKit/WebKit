<?php
    $policy = $_GET["policy"];
    $to = $_GET["to"] == "http" ? "http://127.0.0.1:8000" : "https://127.0.0.1:8443";
    header("Content-Security-Policy: referrer $policy");
?>
<!DOCTYPE html>
<html>
<head>
    <script>
        document.location = "<?php echo $to ?>/security/contentSecurityPolicy/resources/referrer-test-endpoint.php";
    </script>
</head>
</html>
