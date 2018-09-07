<?php

header('Content-Type: text/plain');
echo 'User: ' . $_SERVER['PHP_AUTH_USER'], ' Password: ' . $_SERVER['PHP_AUTH_PW'];
