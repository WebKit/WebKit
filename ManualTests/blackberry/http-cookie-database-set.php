<?php if (isset($_COOKIE["db_cookie"])) {
              if ($_COOKIE["db_cookie"] == "https_cookie")
                  echo 'PASS';
              else
                  echo 'FAIL';
      } else {
          header("Set-Cookie: db_cookie = http_cookie; expires=Thu, 12-Apr-2312 08:32:29 GMT;");
          $path = str_replace("http-cookie-database-set.php", "http-cookie-database-update.php", $_SERVER["PHP_SELF"]);
          $newurl = 'https://'.$_SERVER["HTTP_HOST"].$path;
          $location = 'Location:'.$newurl;
          header($location);
      }
?>

