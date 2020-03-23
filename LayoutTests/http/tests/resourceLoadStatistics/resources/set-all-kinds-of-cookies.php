<?php
header("HTTP/1.1 200 OK");

setcookie("persistentCookie", "1", time() + 300, "/");
setcookie("persistentSecureCookie", "1", time() + 300, "/", "", TRUE);
setcookie("persistentHttpOnlyCookie", "1", time() + 300, "/", "", FALSE, TRUE);
setcookie("persistentSameSiteLaxCookie", "1", ["expires" => time() + 300, "path" => "/", "samesite" => "Lax"]);
setcookie("persistentSameSiteStrictCookie", "1", ["expires" => time() + 300, "path" => "/", "samesite" => "Strict"]);

setcookie("sessionCookie", "1", "", "/");
setcookie("sessionSecureCookie", "1", "", "/", "", TRUE);
setcookie("sessionHttpOnlyCookie", "1", "", "/", "", FALSE, TRUE);
setcookie("sessionSameSiteLaxCookie", "1", ["path" => "/", "samesite" => "Lax"]);
setcookie("sessionSameSiteStrictCookie", "1", ["path" => "/", "samesite" => "Strict"]);
?>
