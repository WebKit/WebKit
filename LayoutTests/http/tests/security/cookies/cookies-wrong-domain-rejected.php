<?php
    header('Set-Cookie: one_cookie=shouldBeRejeced; domain=WrongDomain');
    header('Location: cookies-wrong-domain-rejected-result.php');
?>
