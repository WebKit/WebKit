.. _multiple-bz-dbs:

One Installation, Multiple Instances
####################################

This is a somewhat specialist feature; if you don't know whether you need it,
you don't. It is useful to admins who want to run many separate instances of
Bugzilla from a single installed codebase.

This is possible by using the ``PROJECT`` environment variable. When accessed,
Bugzilla checks for the existence of this variable, and if present, uses
its value to check for an alternative configuration file named
:file:`localconfig.<PROJECT>` in the same location as
the default one (:file:`localconfig`). It also checks for
customized templates in a directory named
:file:`<PROJECT>` in the same location as the
default one (:file:`template/<langcode>`). By default
this is :file:`template/en/default` so ``PROJECT``'s templates
would be located at :file:`template/en/PROJECT`.

To set up an alternate installation, just export ``PROJECT=foo`` before
running :command:`checksetup.pl` for the first time. It will
result in a file called :file:`localconfig.foo` instead of
:file:`localconfig`. Edit this file as described above, with
reference to a new database, and re-run :command:`checksetup.pl`
to populate it. That's all.

Now you have to configure the web server to pass this environment
variable when accessed via an alternate URL, such as virtual host for
instance. The following is an example of how you could do it in Apache,
other Webservers may differ.

.. code-block:: apache

    <VirtualHost 12.34.56.78:80>
        ServerName bugzilla.example.com
        SetEnv PROJECT foo
    </VirtualHost>

Don't forget to also export this variable before accessing Bugzilla
by other means, such as repeating tasks like those above.
