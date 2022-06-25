<?php
/*
Plugin Name: Google Analytics Plugin
Plugin URI: http://webkit.org
Description: Adds Google Analytics for webkit.org
Author: Jonathan Davis
Version: 1.0
*/

add_action( 'wp_head', function() {?>
    <script type="text/javascript">
        (function(i,s,o,g,r,a,m){i['GoogleAnalyticsObject']=r;i[r]=i[r]||function(){(i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();a=s.createElement(o),m=s.getElementsByTagName(o)[0];a.async=1;a.src=g;m.parentNode.insertBefore(a,m)})(window,document,'script','//www.google-analytics.com/analytics.js','ga'); ga('create', 'UA-7299333-1', 'auto'); ga('send', 'pageview');
    </script>
<?php
} );