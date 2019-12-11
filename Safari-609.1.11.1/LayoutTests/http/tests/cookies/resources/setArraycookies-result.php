<html>
<head>
<script>
    function run(){
        // Reading the Cooikes using PHP
        var cookie = "<?php if (isset($_COOKIE['setArraycookie'])) {
                                foreach ($_COOKIE['setArraycookie'] as $name => $value) {
                                    $name = htmlspecialchars($name);
                                    $value = htmlspecialchars($value);
                                    echo "$name : $value";
                                }
		            } 
                     ?>";

        var status = "Fail";
        if (cookie == "three : cookiethreetwo : cookietwoone : cookieone") {
            document.getElementById("test").innerHTML = "<b>Passed</b>";
            status = "Pass";
        } else
            document.getElementById("test").innerHTML = "<b>Failed</b>";    
    }
if (window.testRunner)
        testRunner.dumpAsText();

</script>
</head>
<body onload="run()">
<p>This test case to set a Array of Cookie size of 3 and check if it's been correctly set and in order.<br />If the Test case was successful, Then it should print Passed below!! </p><br/>
<div id="test">
Not Working!!
</div>
</body>
</html>

