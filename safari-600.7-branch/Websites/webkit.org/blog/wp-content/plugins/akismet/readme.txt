=== Akismet ===
Contributors: matt, ryan, andy, mdawaffe, tellyworth, josephscott, lessbloat, automattic
Tags: akismet, comments, spam
Requires at least: 3.0
Tested up to: 3.1
Stable tag: 2.5.3
License: GPLv2 or later

Akismet checks your comments against the Akismet web service to see if they look like spam or not.

== Description ==

Akismet checks your comments against the Akismet web service to see if they look like spam or not and lets you
review the spam it catches under your blog's "Comments" admin screen.

Major new features in Akismet 2.5 include:

* A comment status history, so you can easily see which comments were caught or cleared by Akismet, and which were spammed or unspammed by a moderator
* Links are highlighted in the comment body, to reveal hidden or misleading links
* If your web host is unable to reach Akismet's servers, the plugin will automatically retry when your connection is back up
* Moderators can see the number of approved comments for each user
* Spam and Unspam reports now include more information, to help improve accuracy

PS: You'll need an [Akismet.com API key](http://akismet.com/get/) to use it.  Keys are free for personal blogs, with paid subscriptions available for businesses and commercial sites.

== Installation ==

Upload the Akismet plugin to your blog, Activate it, then enter your [Akismet.com API key](http://akismet.com/get/).

1, 2, 3: You're done!

== Changelog ==

= 2.5.3 = 
* Specify the license is GPL v2 or later
* Fix a bug that could result in orphaned commentmeta entries
* Include hotfix for WordPress 3.0.5 filter issue

= 2.5.2 =

* Properly format the comment count for author counts
* Look for super admins on multisite installs when looking up user roles
* Increase the HTTP request timeout
* Removed padding for author approved count
* Fix typo in function name
* Set Akismet stats iframe height to fixed 2500px.  Better to have one tall scroll bar than two side by side.

= 2.5.1 =

* Fix a bug that caused the "Auto delete" option to fail to discard comments correctly
* Remove the comment nonce form field from the 'Akismet Configuration' page in favor of using a filter, akismet_comment_nonce
* Fixed padding bug in "author" column of posts screen
* Added margin-top to "cleared by ..." badges on dashboard
* Fix possible error when calling akismet_cron_recheck()
* Fix more PHP warnings
* Clean up XHTML warnings for comment nonce
* Fix for possible condition where scheduled comment re-checks could get stuck
* Clean up the comment meta details after deleting a comment
* Only show the status badge if the comment status has been changed by someone/something other than Akismet
* Show a 'History' link in the row-actions
* Translation fixes
* Reduced font-size on author name
* Moved "flagged by..." notification to top right corner of comment container and removed heavy styling
* Hid "flagged by..." notification while on dashboard

= 2.5.0 =

* Track comment actions under 'Akismet Status' on the edit comment screen
* Fix a few remaining deprecated function calls ( props Mike Glendinning ) 
* Use HTTPS for the stats IFRAME when wp-admin is using HTTPS
* Use the WordPress HTTP class if available
* Move the admin UI code to a separate file, only loaded when needed
* Add cron retry feature, to replace the old connectivity check
* Display Akismet status badge beside each comment
* Record history for each comment, and display it on the edit page
* Record the complete comment as originally submitted in comment_meta, to use when reporting spam and ham
* Highlight links in comment content
* New option, "Show the number of comments you've approved beside each comment author."
* New option, "Use a nonce on the comment form."

= 2.4.0 =

* Spell out that the license is GPLv2
* Fix PHP warnings
* Fix WordPress deprecated function calls
* Fire the delete_comment action when deleting comments
* Move code specific for older WP versions to legacy.php
* General code clean up

= 2.3.0 =

* Fix "Are you sure" nonce message on config screen in WPMU
* Fix XHTML compliance issue in sidebar widget
* Change author link; remove some old references to WordPress.com accounts
* Localize the widget title (core ticket #13879)

= 2.2.9 =

* Eliminate a potential conflict with some plugins that may cause spurious reports

= 2.2.8 =

* Fix bug in initial comment check for ipv6 addresses
* Report comments as ham when they are moved from spam to moderation
* Report comments as ham when clicking undo after spam
* Use transition_comment_status action when available instead of older actions for spam/ham submissions
* Better diagnostic messages when PHP network functions are unavailable
* Better handling of comments by logged-in users

= 2.2.7 =

* Add a new AKISMET_VERSION constant
* Reduce the possibility of over-counting spam when another spam filter plugin is in use
* Disable the connectivity check when the API key is hard-coded for WPMU

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
