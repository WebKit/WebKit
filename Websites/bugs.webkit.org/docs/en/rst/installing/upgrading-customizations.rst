.. _upgrading-customizations:

Upgrading a Customized or Extended Bugzilla
###########################################

If your Bugzilla has been customized or uses extensions, you will need to
make your customizations or extensions work with your new version of Bugzilla.
If this is the case, you are particularly strongly recommended to do a test
upgrade on a test system and use that to help you port forward your
customizations.

If your extension came from a third party, look to see if an updated version
is available for the version of Bugzilla you are upgrading to. If not, and
you want to continue using it, you'll need to port it forward yourself.

If you are upgrading from a version of Bugzilla earlier than 3.6 and have
extensions for which a newer version is not available from an upstream source,
then you need to convert them. This is because the extension format changed
in version 3.6. There is a file called :file:`extension-convert.pl` in the
:file:`contrib` directory which may be able to help you with that.
