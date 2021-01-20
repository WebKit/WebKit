var referrerHeader = "<?php echo $_SERVER['HTTP_REFERER'] ?>";
if (referrerHeader === "")
    scriptReferrer = "none";
else
    scriptReferrer = referrerHeader;