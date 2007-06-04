=== WP-Cache ===

Tags: caching, performance
Contributors: gallir

WP-Cache is an extremely efficient WordPress page caching system to make you site much faster and responsive. It works by caching  Worpress pages and storing them in a  static file for serving future requests directly from the file rather than loading and compiling the whole PHP code and the building the page from the database. WP-Cache allows to serve hundred of times more pages per second, and to reduce the response time from several tenths of seconds to less than a millisecond.

WP-Cache started from the '''Staticize Reloaded''' by matt and billzeller. Most of their recommendatiosn also apply to WP-Cache. Current version, WP-Cache2, is a huge improvement over previous versions of WP-Cache.

WP-Cache is composed of two parts:

1. Two-phases Wordpress hooks. The first is called at the very begining  --wp-cache-phase1.php-- when just few code has been compiled. The second --wp-cache-phase2.php-- after all plugins have been executed. The first phase checks if the requested URL is already cached, if so it serves from the static file and finishes. The second phase stores the generated page in a static file for further request.

2. The WP-Cache plugin. This plugin configures and manages the whole process. It is easy to use and self-documented. It allows yoy to enable or disable cache, define expiration time for static pages, define rules for rejecting and accepting which URLs and php files can be cached, and shows and delete pages in cache.

== Installation ==

1. Upload to your plugins folder, usually `wp-content/plugins/` and unzip the file, it will create a `wp-content/plugins/wp-cache/` directory.

2. If you have Compression turned on under Miscellaneous options, turn it off.

3. Activate the plugin on the plugin screen.

4. Go to "Options" administration menu, select "WP-Cache" from the submenu, the plugin will try to autoconfigure everything.  The plugin will try to autoconfigure everything, in case of failure --normally due to the lack of files' permissions-- it tell you and give the instructions to solve the problems.

== Manual Installation ==

In case you don't want to make `wp-config.php` writeable by the server, or you want to make all steps manually:


1. Upload to your plugins folder, usually `wp-content/plugins/` and unzip the file, it will create a `wp-content/plugins/wp-cache/` directory.

2. If you have Compression turned on under Miscellaneous options, turn it off.

3. Create `wp-content/cache`directory and make sure the web server can write in it.

4. Make `wp-content` directory writeable by the web server.

5. Create a symbolic link from wp-content/advanced-cache.php to wp-content/plugins/wp-cache/wp-cache-phase1.php.

    ln -s   wp-content/plugins/wp-cache/wp-cache-phase1.php wp-content/advanced-cache.php

6. Add the following line to you wp-config.php` file:

    define('WP_CACHE', true);

7. Go to "Options" administration menu, select "WP-Cache" from the submenu and enable cache.

== Frequently Asked Questions ==

= Do I really need to use this plugin? =


From "Staticize Reloaded". Probably not, WordPress is fast enough that caching usually only adds a few milliseconds of performance that isn't really perceptible by users. Some reasons you may want to use Staticize Reloaded:

* If your site gets Slashdotted
* If you're on a very slow server
* If you've had a complaint from your host about performance

= How can I tell if it's working? =

WP-Cache adds some stats to the very end of a page in the HTML, so you can view source to see the time it took to generate a page and rather it was cached or not. Remember that the cache is created on demand, so the first time you load a page it'll be generated from the database. This first time WP-Cache will add  a lines with the total time needed to create the page, if the page is directly served from the cache, it adds a second line telling it was served from cache.

= I see gibberish on the screen when I activate this plugin? =

Make sure that you deactivated compression on the Miscellaneous options screen and that gzip encoding is turned off on the PHP level.

= How do I make certain pages stay dynamic? =

You can specify rules that reject request based on URI strings and also specify acceptable files for caching.


= How do I make certain parts of the page stay dynamic? =

It's compatible with Staticze Reloaded. From their FAQ:

There are two ways to do this, you can have functions that say dynamic or include entire other files. To have a dynamic function in the cached PHP page use this syntax around the function:

`<!--mfunc function_name('parameter', 'another_parameter') -->
<?php function_name('parameter', 'another_parameter') ?>
<!--/mfunc-->`

The HTML comments around the mirrored PHP allow it to be executed in the static page. To include another file try this:

`<!--mclude file.php-->
<?php include_once(ABSPATH . 'file.php'); ?>
<!--/mclude-->`

That will include file.php under the ABSPATH directory, which is the same as where your `wp-config.php` file is located.


= I removed WP-Cache and now Wordpress does not work =

This occurs because Wordpress still try to run `advanced-cache.php`. Remove the following line to you wp-config.php` file:

    define('WP_CACHE', true);


== Screenshots ==

1. This is a chart showing the performance of a WordPress blog under very high loads with and without Staticize. It was taken similarly to [http://weblogtoolscollection.com/archives/2004/07/26/staticise-analysis/ this post].

[http://mnm.uib.es/gallir/wp-content/load.png  Average load]

[http://mnm.uib.es/gallir/wp-content/load-log.png  Average load, logarithmic scale]

[http://mnm.uib.es/gallir/wp-content/wpcache21.png  WP-Cache plugin]

[http://mnm.uib.es/gallir/wp-content/wpcache23.png  WP-Cache plugin, listing files in cache]

== More Info ==

For more info, please visit [http://mnm.uib.es/gallir/wp-cache-2/ WP-Cache home page].

(For feedback/comments, please send an e-mail to: gallir@uib.es ).

