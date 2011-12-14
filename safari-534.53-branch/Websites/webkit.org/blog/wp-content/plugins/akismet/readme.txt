=== Akismet ===
Contributors: matt, ryan, andy, mdawaffe, tellyworth
Tags: akismet, comments, spam
Requires at least: 2.0
Tested up to: 2.8.2

Akismet checks your comments against the Akismet web service to see if they look like spam or not.

== Description ==

Akismet checks your comments against the Akismet web service to see if they look like spam or not and lets you
review the spam it catches under your blog's "Comments" admin screen.

Want to show off how much spam Akismet has caught for you? Just put `<?php akismet_counter(); ?>` in your template.

See also: [WP Stats plugin](http://wordpress.org/extend/plugins/stats/).

PS: You'll need a [WordPress.com API key](http://wordpress.com/api-keys/) to use it.

== Installation ==

Upload the Akismet plugin to your blog, Activate it, then enter your [WordPress.com API key](http://wordpress.com/api-keys/).

1, 2, 3: You're done!

== Changelog ==

= 2.2.7 =

* Add a new AKISMET_VERSION constant

= 2.2.6 =

* Fix a global warning introduced in 2.2.5
* Add changelog and additional readme.txt tags
* Fix an array conversion warning in some versions of PHP
* Support a new WPCOM_API_KEY constant for easier use with WordPress MU

= 2.2.5 =

* Include a new Server Connectivity diagnostic check, to detect problems caused by firewalls

= 2.2.4 =

* Fixed a key problem affecting the stats feature in WordPress MU
* Provide additional blog information in Akismet API calls
