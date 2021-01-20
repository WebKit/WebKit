==============================
The WebKit Linux Developer SDK
==============================

This is aimed to replace the JHBuild setup, as announced in `webkit-dev`_. Most
of the time developers won't need to manually build the SDK, they will download
already-built versions of it.

Rebuilding the SDK is needed when you have to update WebKit test dependencies or
add new dependencies to the SDK.

To build the SDK, run:

::

  $ Tools/Scripts/webkit-flatpak-sdk --build

This can take hours and use gigabytes of your hard-drive. To build a single recipe, use something
like:

::

  $ Tools/Scripts/bst-wrapper build sdk/gtk.bst

To enter the Buildstream SDK shell and debug stuff when hacking on a specific
dependency:

::

  $ Tools/Scripts/bst-wrapper shell sdk/gtk.bst

To test your changes in WebKit:

::

  $ Tools/Scripts/webkit-flatpak -u --repo=$PWD/Tools/buildstream/repo
  $ Tools/Scripts/run-minibrowser --gtk --flatpak-repo=$PWD/Tools/buildstream/repo <url>
  $ Tools/Scripts/run-webkit-tests --gtk --repo=$PWD/Tools/buildstream/repo
  ...

bst-wrapper has the same options as bst itself, documented on the `Buildstream website`_.

Recipe version updates should follow this procedure:

- For recipes tracking git tags (with the `git_tag` source kind), you can manually update the branch
  or tag name referenced in the `track` value (see rr.bst for example). and then run `bst-wrapper
  track sdk/<recipe>.bst`. If the recipe tracks git master, no manual update is needed, just run the
  track command.
- For recipes tracking tarballs, update the `url` value and run `bst-wrapper track
  sdk/<recipe>.bst`.


Another nice feature of Buildstream is `workspace`, where you can make changes
to recipes:

::

  $ Tools/Scripts/bst-wrapper workspace open sdk/openxr.bst ~/Projects/openxr
  # hack hack hack in ~/Projects/openxr/
  $ Tools/Scripts/bst-wrapper build sdk/openxr.bst
  # Once you are happy, format changes from your workspace with git:
  $ cd ~/Projects/openxr
  $ git commit -a ...
  $ git format-patch -1
  $ mv *.patch ~/WebKit/Tools/buildstream/patches/
  # finally add patches as sources in openxr.bst
  # and close the workspace
  $ Tools/Scripts/bst-wrapper workspace close sdk/openxr.bst
  $ rm -fr ~/Projects/openxr

Internally the bst-wrapper will install Buildstream and a few other python
dependencies in a local virtualenv managed by pipenv. This is all abstracted
though, in theory direct access to the pipenv environment shouldn't be needed.


.. _webkit-dev: https://lists.webkit.org/pipermail/webkit-dev/2020-March/031147.html
.. _Buildstream website: https://docs.buildstream.build/1.4.2/index.html
