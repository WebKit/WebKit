<?php
require('/var/www/config/webkit-blog-config.php');

define('WP_CACHE', true); //Added by WP-Cache Manager
define('ABSPATH', dirname(__FILE__).'/');
require_once(ABSPATH.'wp-settings.php');
?>
