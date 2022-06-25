Languages
=========

Bugzilla's templates can be localized, although it's a `big job
<https://wiki.mozilla.org/Bugzilla:L10n:Guide>`_. If you have
a localized set of templates for your version of Bugzilla, Bugzilla can 
support multiple languages at once. In that case, Bugzilla honours the user's
``Accept-Language`` HTTP header when deciding which language to serve. If
multiple languages are installed, a menu will display in the header allowing
the user to manually select a different language. If they do this, their
choice will override the ``Accept-Language`` header.

Many language templates can be obtained from
`the localization section of the Bugzilla website
<http://www.bugzilla.org/download.html#localizations>`_. Instructions
for submitting new languages are also available from that location. There's
also a `list of localization teams
<https://wiki.mozilla.org/Bugzilla:L10n:Localization_Teams>`_; you might
want to contact someone to ask about the status of their localization.
