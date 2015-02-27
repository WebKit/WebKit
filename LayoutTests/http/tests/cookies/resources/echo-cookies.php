<?php

function echoCookie($value, $name)
{
    echo "$name = $value\n";
}

echo "Cookies are:\n";
array_walk($_COOKIE, echoCookie);    

?>
