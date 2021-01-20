<?php

header("Content-type: text/plain");
echo !empty($_SERVER['HTTP_REFERER']) ? $_SERVER['HTTP_REFERER'] : "NO REFERER";
