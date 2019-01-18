.. _skins:

Skins
=====

Bugzilla supports skins - ways of changing the look of the UI without altering
its underlying structure. It ships with two - "Classic" and "Dusk". You can
find some more listed
`on the wiki <https://wiki.mozilla.org/Bugzilla:Addons#Skins>`_, and there
are a couple more which are part of
`bugzilla.mozilla.org <https://github.com/mozilla-bteam/bmo>`_.
However, in each
case you may need to check that the skin supports the version of Bugzilla
you have. 

To create a new custom skin, make a directory that contains all the same CSS
file names as :file:`skins/standard/`, and put your directory in
:file:`skins/contrib/`. Then, add your CSS to the appropriate files.

After you put the directory there, make sure to run :file:`checksetup.pl` so
that it can set the file permissions correctly.

After you have installed the new skin, it will show up as an option in the
user's :guilabel:`Preferences`, on the :guilabel:`General` tab. If you would
like to force a particular skin on all users, just select that skin in the
:guilabel:`Default Preferences` in the :guilabel:`Administration` UI, and
then uncheck "Enabled" on the preference, so users cannot change it.
