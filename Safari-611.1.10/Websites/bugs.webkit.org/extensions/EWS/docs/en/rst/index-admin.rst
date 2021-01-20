EWS
#########

The EWS extension provides a mechanism to automatically CC the feeder EWS on restricted bugs
(e.g. security bugs) that have unreviewed patches. In this way the feeder EWS account can be
unprivileged. That is, it only needs to have access to publicly visible bugs.

===================================================
Installing this extension
===================================================

Copy the directory that contains the docs subdirectory that this file is under into the Bugzilla
extension directory. Then run ./checksetup.pl from the top-level Bugzilla installation directory.

===================================================
Configuring the feeder EWS account to use
===================================================

Login to Bugzilla as an administrator, click Administration in the header, then Parameters, and
then EWS. Set the parameter ews_feeder_login to the login name of the feeder EWS account. Then
click Save Changes.

Note that setting ews_feeder_login to the empty string will effectively disable the extension
though the extension will still be loaded.
