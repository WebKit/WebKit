.. _parameters:

Parameters
##########

Bugzilla is configured by changing various parameters, accessed
from the :guilabel:`Parameters` link, which is found on the Administration
page. The parameters are divided into several categories,
accessed via the menu on the left.

.. _param-required-settings:

Required Settings
=================

The core required parameters for any Bugzilla installation are set
here. :param:`urlbase` is always required; the other parameters should be
set, or it must be explicitly decided not to
set them, before the new Bugzilla installation starts to be used.

urlbase
    Defines the fully qualified domain name and web
    server path to this Bugzilla installation.
    For example, if the Bugzilla query page is
    :file:`http://www.foo.com/bugzilla/query.cgi`,
    the :param:`urlbase` should be set
    to :paramval:`http://www.foo.com/bugzilla/`.

ssl_redirect
    If enabled, Bugzilla will force HTTPS (SSL) connections, by
    automatically redirecting any users who try to use a non-SSL
    connection. Also, when this is enabled, Bugzilla will send out links
    using :param:`sslbase` in emails instead of :param:`urlbase`.

sslbase
    Defines the fully qualified domain name and web
    server path for HTTPS (SSL) connections to this Bugzilla installation.
    For example, if the Bugzilla main page is
    :file:`https://www.foo.com/bugzilla/index.cgi`,
    the :param:`sslbase` should be set
    to :paramval:`https://www.foo.com/bugzilla/`.

cookiepath
    Defines a path, relative to the web document root, that Bugzilla
    cookies will be restricted to. For example, if the
    :param:`urlbase` is set to
    :file:`http://www.foo.com/bugzilla/`, the
    :param:`cookiepath` should be set to
    :paramval:`/bugzilla/`. Setting it to :paramval:`/` will allow all sites
    served by this web server or virtual host to read Bugzilla cookies.

.. _param-general:

General
=======

maintainer
    Email address of the person
    responsible for maintaining this Bugzilla installation.
    The address need not be that of a valid Bugzilla account.

utf8
    Use UTF-8 (Unicode) encoding for all text in Bugzilla. Installations where
    this parameter is set to :paramval:`off` should set it to :paramval:`on` only
    after the data has been converted from existing legacy character
    encodings to UTF-8, using the
    :file:`contrib/recode.pl` script.

    .. note:: If you turn this parameter from :paramval:`off` to :paramval:`on`,
              you must re-run :file:`checksetup.pl` immediately afterward.

shutdownhtml
    If there is any text in this field, this Bugzilla installation will
    be completely disabled and this text will appear instead of all
    Bugzilla pages for all users, including Admins. Used in the event
    of site maintenance or outage situations.

announcehtml
    Any text in this field will be displayed at the top of every HTML
    page in this Bugzilla installation. The text is not wrapped in any
    tags. For best results, wrap the text in a ``<div>``
    tag. Any style attributes from the CSS can be applied. For example,
    to make the text green inside of a red box, add ``id=message``
    to the ``<div>`` tag.

upgrade_notification
    Enable or disable a notification on the homepage of this Bugzilla
    installation when a newer version of Bugzilla is available. This
    notification is only visible to administrators. Choose :paramval:`disabled`
    to turn off the notification. Otherwise, choose which version of
    Bugzilla you want to be notified about: :paramval:`development_snapshot` is the
    latest release from the master branch, :paramval:`latest_stable_release` is the most
    recent release available on the most recent stable branch, and
    :paramval:`stable_branch_release` is the most recent release on the branch
    this installation is based on.

.. _param-administrative-policies:

Administrative Policies
=======================

This page contains parameters for basic administrative functions.
Options include whether to allow the deletion of bugs and users,
and whether to allow users to change their email address.

allowbugdeletion
    The pages to edit products and components can delete all associated bugs when you delete a product (or component). Since that is a pretty scary idea, you have to turn on this option before any such deletions will ever happen.

allowemailchange
    Users can change their own email address through the preferences. Note that the change is validated by emailing both addresses, so switching this option on will not let users use an invalid address.

allowuserdeletion
    The user editing pages are capable of letting you delete user accounts. Bugzilla will issue a warning in case you'd run into inconsistencies when you're about to do so, but such deletions still remain scary. So, you have to turn on this option before any such deletions will ever happen.

last_visit_keep_days
    This option controls how many days Bugzilla will remember that users have visited specific bugs.

.. _param-user-authentication:

User Authentication
===================

This page contains the settings that control how this Bugzilla
installation will do its authentication. Choose what authentication
mechanism to use (the Bugzilla database, or an external source such
as LDAP), and set basic behavioral parameters. For example, choose
whether to require users to login to browse bugs, the management
of authentication cookies, and the regular expression used to
validate email addresses. Some parameters are highlighted below.

auth_env_id
    Environment variable used by external authentication system to store a unique identifier for each user. Leave it blank if there isn't one or if this method of authentication is not being used.

auth_env_email
    Environment variable used by external authentication system to store each user's email address. This is a required field for environmental authentication. Leave it blank if you are not going to use this feature.

auth_env_realname
    Environment variable used by external authentication system to store the user's real name. Leave it blank if there isn't one or if this method of authentication is not being used.

user_info_class
    Mechanism(s) to be used for gathering a user's login information. More than one may be selected. If the first one returns nothing, the second is tried, and so on. The types are:

    * :paramval:`CGI`: asks for username and password via CGI form interface.
    * :paramval:`Env`: info for a pre-authenticated user is passed in system environment variables.

user_verify_class
    Mechanism(s) to be used for verifying (authenticating) information gathered by user_info_class. More than one may be selected. If the first one cannot find the user, the second is tried, and so on. The types are:

    * :paramval:`DB`: Bugzilla's built-in authentication. This is the most common choice.
    * :paramval:`RADIUS`: RADIUS authentication using a RADIUS server. Using this method requires additional parameters to be set. Please see :ref:`param-radius` for more information.
    * :paramval:`LDAP`: LDAP authentication using an LDAP server. Using this method requires additional parameters to be set. Please see :ref:`param-ldap` for more information.

rememberlogin
    Controls management of session cookies.

    * :paramval:`on` - Session cookies never expire (the user has to login only once per browser).
    * :paramval:`off` - Session cookies last until the users session ends (the user will have to login in each new browser session).
    * :paramval:`defaulton`/:paramval:`defaultoff` - Default behavior as described above, but user can choose whether Bugzilla will remember their login or not.

requirelogin
    If this option is set, all access to the system beyond the front page will require a login. No anonymous users will be permitted.

webservice_email_filter
    Filter email addresses returned by the WebService API depending on if the user is logged in or not. This works similarly to how the web UI currently filters email addresses. If requirelogin is enabled, then this parameter has no effect as users must be logged in to use Bugzilla anyway.

emailregexp
    Defines the regular expression used to validate email addresses
    used for login names. The default attempts to match fully
    qualified email addresses (i.e. 'user\@example.com') in a slightly
    more restrictive way than what is allowed in RFC 2822.
    Another popular value to put here is :paramval:`^[^@]+`, which means 'local usernames, no @ allowed.'

emailregexpdesc
    This description is shown to the user to explain which email addresses are allowed by the :param:`emailregexp` param.

emailsuffix
    This is a string to append to any email addresses when actually sending mail to that address. It is useful if you have changed the :param:`emailregexp` param to only allow local usernames, but you want the mail to be delivered to username\@my.local.hostname.

createemailregexp
    This defines the (case-insensitive) regexp to use for email addresses that are permitted to self-register. The default (:paramval:`.*`) permits any account matching the emailregexp to be created. If this parameter is left blank, no users will be permitted to create their own accounts and all accounts will have to be created by an administrator.

password_complexity
    Set the complexity required for passwords. In all cases must the passwords be at least 6 characters long.

    * :paramval:`no_constraints` - No complexity required.
    * :paramval:`mixed_letters` - Passwords must contain at least one UPPER and one lower case letter.
    * :paramval:`letters_numbers` - Passwords must contain at least one UPPER and one lower case letter and a number.
    * :paramval:`letters_numbers_specialchars` - Passwords must contain at least one letter, a number and a special character.

password_check_on_login
    If set, Bugzilla will check that the password meets the current complexity rules and minimum length requirements when the user logs into the Bugzilla web interface. If it doesn't, the user would not be able to log in, and will receive a message to reset their password.

.. _param-attachments:

Attachments
===========

This page allows for setting restrictions and other parameters
regarding attachments to bugs. For example, control size limitations
and whether to allow pointing to external files via a URI.

allow_attachment_display
    If this option is on, users will be able to view attachments from their browser, if their browser supports the attachment's MIME type. If this option is off, users are forced to download attachments, even if the browser is able to display them.

    If you do not trust your users (e.g. if your Bugzilla is public), you should either leave this option off, or configure and set the :param:`attachment_base` parameter (see below). Untrusted users may upload attachments that could be potentially damaging if viewed directly in the browser.

attachment_base
    When the :param:`allow_attachment_display` parameter is on, it is possible for a malicious attachment to steal your cookies or perform an attack on Bugzilla using your credentials.

    If you would like additional security on attachments to avoid this, set this parameter to an alternate URL for your Bugzilla that is not the same as :param:`urlbase` or :param:`sslbase`. That is, a different domain name that resolves to this exact same Bugzilla installation.

    Note that if you have set the :param:`cookiedomain` parameter, you should set :param:`attachment_base` to use a domain that would not be matched by :param:`cookiedomain`.

    For added security, you can insert ``%bugid%`` into the URL, which will be replaced with the ID of the current bug that the attachment is on, when you access an attachment. This will limit attachments to accessing only other attachments on the same bug. Remember, though, that all those possible domain names (such as 1234.your.domain.com) must point to this same Bugzilla instance. To set this up you need to investigate wildcard DNS.

allow_attachment_deletion
    If this option is on, administrators will be able to delete the contents
    of attachments (i.e. replace the attached file with a 0 byte file),
    leaving only the metadata.

maxattachmentsize
    The maximum size (in kilobytes) of attachments to be stored in the database. If a file larger than this size is attached to a bug, Bugzilla will look at the :param:`maxlocalattachment` parameter to determine if the file can be stored locally on the web server. If the file size exceeds both limits, then the attachment is rejected. Setting both parameters to 0 will prevent attaching files to bugs.

    Some databases have default limits which prevent storing larger attachments in the database. E.g. MySQL has a parameter called `max_allowed_packet <http://dev.mysql.com/doc/refman/5.1/en/packet-too-large.html>`_, whose default varies by distribution. Setting :param:`maxattachmentsize` higher than your current setting for this value will produce an error.

maxlocalattachment
    The maximum size (in megabytes) of attachments to be stored locally on the web server. If set to a value lower than the :param:`maxattachmentsize` parameter, attachments will never be kept on the local filesystem.

    Whether you use this feature or not depends on your environment. Reasons to store some or all attachments as files might include poor database performance for large binary blobs, ease of backup/restore/browsing, or even filesystem-level deduplication support. However, you need to be aware of any limits on how much data your webserver environment can store. If in doubt, leave the value at 0.

    Note that changing this value does not affect any already-submitted attachments.

.. _param-bug-change-policies:

Bug Change Policies
===================

Set policy on default behavior for bug change events. For example,
choose which status to set a bug to when it is marked as a duplicate,
and choose whether to allow bug reporters to set the priority or
target milestone. Also allows for configuration of what changes
should require the user to make a comment, described below.

duplicate_or_move_bug_status
    When a bug is marked as a duplicate of another one, use this bug status.

letsubmitterchoosepriority
    If this is on, then people submitting bugs can choose an initial priority for that bug. If off, then all bugs initially have the default priority selected here.

letsubmitterchoosemilestone
    If this is on, then people submitting bugs can choose the Target Milestone for that bug. If off, then all bugs initially have the default milestone for the product being filed in.

musthavemilestoneonaccept
    If you are using Target Milestone, do you want to require that the milestone be set in order for a user to set a bug's status to IN_PROGRESS?

commenton*
    All these fields allow you to dictate what changes can pass
    without comment and which must have a comment from the
    person who changed them.  Often, administrators will allow
    users to add themselves to the CC list, accept bugs, or
    change the Status Whiteboard without adding a comment as to
    their reasons for the change, yet require that most other
    changes come with an explanation.
    Set the "commenton" options according to your site policy. It
    is a wise idea to require comments when users resolve, reassign, or
    reopen bugs at the very least.

    .. note:: It is generally far better to require a developer comment
       when resolving bugs than not. Few things are more annoying to bug
       database users than having a developer mark a bug "fixed" without
       any comment as to what the fix was (or even that it was truly
       fixed!)

noresolveonopenblockers
    This option will prevent users from resolving bugs as FIXED if
    they have unresolved dependencies. Only the FIXED resolution
    is affected. Users will be still able to resolve bugs to
    resolutions other than FIXED if they have unresolved dependent
    bugs.

.. _param-bugfields:

Bug Fields
==========

The parameters in this section determine the default settings of
several Bugzilla fields for new bugs and whether
certain fields are used. For example, choose whether to use the
:field:`Target Milestone` field or the :field:`Status Whiteboard` field.

useclassification
    If this is on, Bugzilla will associate each product with a specific
    classification. But you must have :group:`editclassification` permissions
    enabled in order to edit classifications.

usetargetmilestone
    Do you wish to use the :field:`Target Milestone` field?

useqacontact
    This allows you to define an email address for each component,
    in addition to that of the default assignee, that will be sent
    carbon copies of incoming bugs.

usestatuswhiteboard
    This defines whether you wish to have a free-form, overwritable field
    associated with each bug. The advantage of the :field:`Status Whiteboard`
    is that it can be deleted or modified with ease and provides an
    easily searchable field for indexing bugs that have some trait in
    common.

use_see_also
    Do you wish to use the :field:`See Also` field? It allows you mark bugs
    in other bug tracker installations as being related. Disabling this field
    prevents addition of new relationships, but existing ones will continue to
    appear.

defaultpriority
    This is the priority that newly entered bugs are set to.

defaultseverity
    This is the severity that newly entered bugs are set to.

defaultplatform
    This is the platform that is preselected on the bug entry form.
    You can leave this empty; Bugzilla will then use the platform that the
    browser is running on as the default.

defaultopsys
    This is the operating system that is preselected on the bug entry form.
    You can leave this empty; Bugzilla will then use the operating system
    that the browser reports to be running on as the default.

collapsed_comment_tags
    A comma-separated list of tags which, when applied to comments, will
    cause them to be collapsed by default.

.. _param-dependency-graphs:

Graphs
======

Bugzilla can draw graphs of bug-dependency relationships, using a tool called
:file:`dot` (from the `GraphViz project <http://graphviz.org/>`_) or a web
service called Web Dot. This page allows you to set the location of the binary
or service. If no Web Dot server or binary is specified, then dependency
graphs will be disabled.

webdotbase
    You may set this parameter to any of the following:

    * A complete file path to :command:`dot` (part of GraphViz), which will
      generate the graphs locally.
    * A URL prefix pointing to an installation of the Web Dot package, which
      will generate the graphs remotely.
    * A blank value, which will disable dependency graphing.

    The default value is blank. We recommend using a local install of
    :file:`dot`. If you change this value to a web service, make certain that
    the Web Dot server can read files from your Web Dot directory. On Apache
    you do this by editing the :file:`.htaccess` file; for other systems the
    needed measures may vary. You can run :command:`checksetup.pl` to
    recreate the :file:`.htaccess` file if it has been lost.

font_file
    You can specify the full path to a TrueType font file which will be used
    to display text (labels, legends, ...) in charts and graphical reports.
    To support as many languages as possible, we recommend to specify a
    TrueType font such as Unifont which supports all printable characters in
    the Basic Multilingual Plane. If you leave this parameter empty, a default
    font will be used, but its support is limited to English characters only
    and so other characters will be displayed incorrectly. 

.. _param-group-security:

Group Security
==============

Bugzilla allows for the creation of different groups, with the
ability to restrict the visibility of bugs in a group to a set of
specific users. Specific products can also be associated with
groups, and users restricted to only see products in their groups.
Several parameters are described in more detail below. Most of the
configuration of groups and their relationship to products is done
on the :guilabel:`Groups` and :guilabel:`Product` pages of the
:guilabel:`Administration` area.
The options on this page control global default behavior.
For more information on Groups and Group Security, see
:ref:`groups`.

makeproductgroups
    Determines whether or not to automatically create groups
    when new products are created. If this is on, the groups will be
    used for querying bugs.

    .. todo:: This is spectacularly unclear. I have no idea what makeproductgroups
              does - can someone explain it to me? Convert this item into a bug on checkin.

chartgroup
    The name of the group of users who can use the 'New Charts' feature. Administrators should ensure that the public categories and series definitions do not divulge confidential information before enabling this for an untrusted population. If left blank, no users will be able to use New Charts.

insidergroup
    The name of the group of users who can see/change private comments and attachments.

timetrackinggroup
    The name of the group of users who can see/change time tracking information.

querysharegroup
    The name of the group of users who are allowed to share saved
    searches with one another. For more information on using
    saved searches, see :ref:`saved-searches`.

comment_taggers_group
    The name of the group of users who can tag comments. Setting this to empty disables comment tagging.

debug_group
    The name of the group of users who can view the actual SQL query generated when viewing bug lists and reports. Do not expose this information to untrusted users.

usevisibilitygroups
    If selected, user visibility will be restricted to members of
    groups, as selected in the group configuration settings.
    Each user-defined group can be allowed to see members of selected
    other groups.
    For details on configuring groups (including the visibility
    restrictions) see :ref:`edit-groups`.

or_groups
    Define the visibility of a bug which is in multiple groups. If
    this is on (recommended), a user only needs to be a member of one
    of the bug's groups in order to view it. If it is off, a user
    needs to be a member of all the bug's groups. Note that in either
    case, a user's role on the bug (e.g. reporter), if any, may also
    affect their permissions.

.. _param-ldap:

LDAP
====

LDAP authentication is a module for Bugzilla's plugin
authentication architecture. This page contains all the parameters
necessary to configure Bugzilla for use with LDAP authentication.

The existing authentication
scheme for Bugzilla uses email addresses as the primary user ID and a
password to authenticate that user. All places within Bugzilla that
require a user ID (e.g assigning a bug) use the email
address. The LDAP authentication builds on top of this scheme, rather
than replacing it. The initial log-in is done with a username and
password for the LDAP directory. Bugzilla tries to bind to LDAP using
those credentials and, if successful, tries to map this account to a
Bugzilla account. If an LDAP mail attribute is defined, the value of this
attribute is used; otherwise, the :param:`emailsuffix` parameter is appended to
the LDAP username to form a full email address. If an account for this address
already exists in the Bugzilla installation, it will log in to that account.
If no account for that email address exists, one is created at the time
of login. (In this case, Bugzilla will attempt to use the "displayName"
or "cn" attribute to determine the user's full name.) After
authentication, all other user-related tasks are still handled by email
address, not LDAP username. For example, bugs are still assigned by
email address and users are still queried by email address.

.. warning:: Because the Bugzilla account is not created until the first time
   a user logs in, a user who has not yet logged is unknown to Bugzilla.
   This means they cannot be used as an assignee or QA contact (default or
   otherwise), added to any CC list, or any other such operation. One
   possible workaround is the :file:`bugzilla_ldapsync.rb`
   script in the :file:`contrib`
   directory. Another possible solution is fixing :bug:`201069`.

Parameters required to use LDAP Authentication:

user_verify_class (in the Authentication section)
    If you want to list :paramval:`LDAP` here,
    make sure to have set up the other parameters listed below.
    Unless you have other (working) authentication methods listed as
    well, you may otherwise not be able to log back in to Bugzilla once
    you log out.
    If this happens to you, you will need to manually edit
    :file:`data/params.json` and set :param:`user_verify_class` to
    :paramval:`DB`.

LDAPserver
    This parameter should be set to the name (and optionally the
    port) of your LDAP server. If no port is specified, it assumes
    the default LDAP port of 389.
    For example: :paramval:`ldap.company.com`
    or :paramval:`ldap.company.com:3268`
    You can also specify a LDAP URI, so as to use other
    protocols, such as LDAPS or LDAPI. If the port was not specified in
    the URI, the default is either 389 or 636 for 'LDAP' and 'LDAPS'
    schemes respectively.

    .. note:: In order to use SSL with LDAP, specify a URI with "ldaps://".
       This will force the use of SSL over port 636.
       For example, normal LDAP :paramval:`ldap://ldap.company.com`, LDAP over
       SSL :paramval:`ldaps://ldap.company.com`, or LDAP over a UNIX
       domain socket :paramval:`ldapi://%2fvar%2flib%2fldap_sock`.

LDAPstarttls
    Whether to require encrypted communication once a normal LDAP connection
    is achieved with the server.

LDAPbinddn [Optional]
    Some LDAP servers will not allow an anonymous bind to search
    the directory. If this is the case with your configuration you
    should set the :param:`LDAPbinddn` parameter to the user account Bugzilla
    should use instead of the anonymous bind.
    Ex. :paramval:`cn=default,cn=user:password`

LDAPBaseDN
    The location in
    your LDAP tree that you would like to search for email addresses.
    Your uids should be unique under the DN specified here.
    Ex. :paramval:`ou=People,o=Company`

LDAPuidattribute
    The attribute
    which contains the unique UID of your users. The value retrieved
    from this attribute will be used when attempting to bind as the
    user to confirm their password.
    Ex. :paramval:`uid`

LDAPmailattribute
    The name of the
    attribute which contains the email address your users will enter
    into the Bugzilla login boxes.
    Ex. :paramval:`mail`

LDAPfilter
    LDAP filter to AND with the LDAPuidattribute for filtering the list of
    valid users.

.. _param-radius:

RADIUS
======

RADIUS authentication is a module for Bugzilla's plugin
authentication architecture. This page contains all the parameters
necessary for configuring Bugzilla to use RADIUS authentication.

.. note:: Most caveats that apply to LDAP authentication apply to RADIUS
   authentication as well. See :ref:`param-ldap` for details.

Parameters required to use RADIUS Authentication:

user_verify_class (in the Authentication section)
    If you want to list :paramval:`RADIUS` here,
    make sure to have set up the other parameters listed below.
    Unless you have other (working) authentication methods listed as
    well, you may otherwise not be able to log back in to Bugzilla once
    you log out.
    If this happens to you, you will need to manually edit
    :file:`data/params.json` and set :param:`user_verify_class` to
    :paramval:`DB`.

RADIUS_server
    The name (and optionally the port) of your RADIUS server.

RADIUS_secret
    The RADIUS server's secret.

RADIUS_NAS_IP
    The NAS-IP-Address attribute to be used when exchanging data with your
    RADIUS server. If unspecified, 127.0.0.1 will be used.

RADIUS_email_suffix
    Bugzilla needs an email address for each user account.
    Therefore, it needs to determine the email address corresponding
    to a RADIUS user.
    Bugzilla offers only a simple way to do this: it can concatenate
    a suffix to the RADIUS user name to convert it into an email
    address.
    You can specify this suffix in the :param:`RADIUS_email_suffix` parameter.
    If this simple solution does not work for you, you'll
    probably need to modify
    :file:`Bugzilla/Auth/Verify/RADIUS.pm` to match your
    requirements.

.. _param-email:

Email
=====

This page contains all of the parameters for configuring how
Bugzilla deals with the email notifications it sends. See below
for a summary of important options.

mail_delivery_method
    This is used to specify how email is sent, or if it is sent at
    all.  There are several options included for different MTAs,
    along with two additional options that disable email sending.
    :paramval:`Test` does not send mail, but instead saves it in
    :file:`data/mailer.testfile` for later review.
    :paramval:`None` disables email sending entirely.

mailfrom
    This is the email address that will appear in the "From" field
    of all emails sent by this Bugzilla installation. Some email
    servers require mail to be from a valid email address; therefore,
    it is recommended to choose a valid email address here.

use_mailer_queue
    In a large Bugzilla installation, updating bugs can be very slow because Bugzilla sends all email at once. If you enable this parameter, Bugzilla will queue all mail and then send it in the background. This requires that you have installed certain Perl modules (as listed by :file:`checksetup.pl` for this feature), and that you are running the :file:`jobqueue.pl` daemon (otherwise your mail won't get sent). This affects all mail sent by Bugzilla, not just bug updates.

smtpserver
    The SMTP server address, if the :param:`mail_delivery_method`
    parameter is set to :paramval:`SMTP`.  Use :paramval:`localhost` if you have a local MTA
    running; otherwise, use a remote SMTP server.  Append ":" and the port
    number if a non-default port is needed.

smtp_username
    Username to use for SASL authentication to the SMTP server.  Leave
    this parameter empty if your server does not require authentication.

smtp_password
    Password to use for SASL authentication to the SMTP server. This
    parameter will be ignored if the :param:`smtp_username`
    parameter is left empty.

smtp_ssl
    Enable SSL support for connection to the SMTP server.

smtp_debug
    This parameter allows you to enable detailed debugging output.
    Log messages are printed the web server's error log.

whinedays
    Set this to the number of days you want to let bugs go
    in the CONFIRMED state before notifying people they have
    untouched new bugs. If you do not plan to use this feature, simply
    do not set up the :ref:`whining cron job <installation-whining>` described
    in the installation instructions, or set this value to "0" (never whine).

globalwatchers
    This allows you to define specific users who will
    receive notification each time any new bug in entered, or when
    any existing bug changes, subject to the normal groupset
    permissions. It may be useful for sending notifications to a
    mailing list, for instance.

.. _param-querydefaults:

Query Defaults
==============

This page controls the default behavior of Bugzilla in regards to
several aspects of querying bugs. Options include what the default
query options are, what the "My Bugs" page returns, whether users
can freely add bugs to the quip list, and how many duplicate bugs are
needed to add a bug to the "most frequently reported" list.

quip_list_entry_control
    Controls how easily users can add entries to the quip list.

    * :paramval:`open` - Users may freely add to the quip list, and their entries will immediately be available for viewing.
    * :paramval:`moderated` - Quips can be entered but need to be approved by a moderator before they will be shown.
    * :paramval:`closed` - No new additions to the quips list are allowed.

mybugstemplate
    This is the URL to use to bring up a simple 'all of my bugs' list
    for a user. %userid% will get replaced with the login name of a
    user. Special characters must be URL encoded.

defaultquery
    This is the default query that initially comes up when you access
    the advanced query page. It's in URL-parameter format.

search_allow_no_criteria
    When turned off, a query must have some criteria specified to limit the number of bugs returned to the user. When turned on, a user is allowed to run a query with no criteria and get all bugs in the entire installation that they can see. Turning this parameter on is not recommended on large installations.

default_search_limit
    By default, Bugzilla limits searches done in the web interface to returning only this many results, for performance reasons. (This only affects the HTML format of search results—CSV, XML, and other formats are exempted.) Users can click a link on the search result page to see all the results.

    Usually you should not have to change this—the default value should be acceptable for most installations.

max_search_results
    The maximum number of bugs that a search can ever return. Tabular and graphical reports are exempted from this limit, however.



.. _param-shadowdatabase:

Shadow Database
===============

This page controls whether a shadow database is used. If your Bugzilla is
not large, you will not need these options.

A standard large database setup involves a single master server and a pool of
read-only slaves (which Bugzilla calls the "shadowdb"). Queries which are not
updating data can be directed to the slave pool, removing the load/locking
from the master, freeing it up to handle writes. Bugzilla will switch to the
shadowdb when it knows it doesn't need to update the database (e.g. when
searching, or displaying a bug to a not-logged-in user).

Bugzilla does not make sure the shadowdb is kept up to date, so, if you use
one, you will need to set up replication in your database server.

If your shadowdb is on a different machine, specify :param:`shadowdbhost`
and :param:`shadowdbport`. If it's on the same machine, specify
:param:`shadowdbsock`.

shadowdbhost
    The host the shadow database is on.

shadowdbport
    The port the shadow database is on.

shadowdbsock
    The socket used to connect to the shadow database, if the host is the
    local machine.

shadowdb
    The database name of the shadow database.

.. _admin-memcached:

Memcached
=========

memcached_servers
    If this option is set, Bugzilla will integrate with `Memcached
    <http://www.memcached.org/>`_. Specify one or more servers, separated by
    spaces, using hostname:port notation (for example:
    :paramval:`127.0.0.1:11211`).

memcached_namespace
    Specify a string to prefix each key on Memcached.

.. _admin-usermatching:

User Matching
=============

The settings on this page control how users are selected and queried
when adding a user to a bug. For example, users need to be selected
when assigning the bug, adding to the CC list, or
selecting a QA contact. With the :param:`usemenuforusers` parameter, it is
possible to configure Bugzilla to
display a list of users in the fields instead of an empty text field.
If users are selected via a text box, this page also
contains parameters for how user names can be queried and matched
when entered.

usemenuforusers
    If this option is set, Bugzilla will offer you a list to select from (instead of a text entry field) where a user needs to be selected. This option should not be enabled on sites where there are a large number of users.

ajax_user_autocompletion
    If this option is set, typing characters in a certain user fields
    will display a list of matches that can be selected from. It is
    recommended to only turn this on if you are using mod_perl;
    otherwise, the response will be irritatingly slow.

maxusermatches
    Provide no more than this many matches when a user is searched for.
    If set to '1', no users will be displayed on ambiguous
    matches. This is useful for user-privacy purposes. A value of zero
    means no limit.

confirmuniqueusermatch
    Whether a confirmation screen should be displayed when only one user matches a search entry.

.. _admin-advanced:

Advanced
========

cookiedomain
    Defines the domain for Bugzilla cookies. This is typically left blank.
    If there are multiple hostnames that point to the same webserver, which
    require the same cookie, then this parameter can be utilized. For
    example, If your website is at
    ``https://bugzilla.example.com/``, setting this to
    :paramval:`.example.com/` will also allow
    ``attachments.example.com/`` to access Bugzilla cookies.

inbound_proxies
    When inbound traffic to Bugzilla goes through a proxy, Bugzilla thinks that the IP address of the proxy is the IP address of every single user. If you enter a comma-separated list of IPs in this parameter, then Bugzilla will trust any ``X-Forwarded-For`` header sent from those IPs, and use the value of that header as the end user's IP address.

proxy_url
    If this Bugzilla installation is behind a proxy, enter the proxy
    information here to enable Bugzilla to access the Internet. Bugzilla
    requires Internet access to utilize the
    :param:`upgrade_notification` parameter. If the
    proxy requires authentication, use the syntax:
    :paramval:`http://user:pass@proxy_url/`.

strict_transport_security
    Enables the sending of the Strict-Transport-Security header along with HTTP responses on SSL connections. This adds greater security to your SSL connections by forcing the browser to always access your domain over SSL and never accept an invalid certificate. However, it should only be used if you have the :param:`ssl_redirect` parameter turned on, Bugzilla is the only thing running on its domain (i.e., your :param:`urlbase` is something like :paramval:`http://bugzilla.example.com/`), and you never plan to stop supporting SSL.

    * :paramval:`off` - Don't send the Strict-Transport-Security header with requests.
    * :paramval:`this_domain_only` - Send the Strict-Transport-Security header with all requests, but only support it for the current domain.
    * :paramval:`include_subdomains` - Send the Strict-Transport-Security header along with the includeSubDomains flag, which will apply the security change to all subdomains. This is especially useful when combined with an :param:`attachment_base` that exists as (a) subdomain(s) under the main Bugzilla domain.
