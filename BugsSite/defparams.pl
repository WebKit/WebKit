# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dawn Endico <endico@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 J. Paul Reed <preed@sigkill.com>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Joseph Heenan <joseph@heenan.me.uk>
#                 Erik Stambaugh <erik@dasbistro.com>
#

# This file defines all the parameters that we have a GUI to edit within
# Bugzilla.

# ATTENTION!!!!   THIS FILE ONLY CONTAINS THE DEFAULTS.
# You cannot change your live settings by editing this file.
# Only adding new parameters is done here.  Once the parameter exists, you
# must use %baseurl%/editparams.cgi from the web to edit the settings.

# This file is included via |do|, mainly because of circular dependency issues
# (such as globals.pl -> Bugzilla::Config -> this -> Bugzilla::Config)
# which preclude compile time loading.

# Those issues may go away at some point, and the contents of this file
# moved somewhere else. Please try to avoid more dependencies from here
# to other code

# (Note that these aren't just added directly to Bugzilla::Config, because
# the backend prefs code is separate to this...)

use strict;
use vars qw(@param_list);
use File::Spec; # for find_languages
use Socket;

use Bugzilla::Config qw(:DEFAULT $templatedir $webdotdir);
use Bugzilla::Util;
use Bugzilla::Constants;

# Checking functions for the various values
# Some generic checking functions are included in Bugzilla::Config

sub check_sslbase {
    my $url = shift;
    if ($url ne '') {
        if ($url !~ m#^https://([^/]+).*/$#) {
            return "must be a legal URL, that starts with https and ends with a slash.";
        }
        my $host = $1;
        if ($host =~ /:\d+$/) {
            return "must not contain a port.";
        }
        local *SOCK;
        my $proto = getprotobyname('tcp');
        socket(SOCK, PF_INET, SOCK_STREAM, $proto);
        my $sin = sockaddr_in(443, inet_aton($host));
        if (!connect(SOCK, $sin)) {
            return "Failed to connect to " . html_quote($host) . 
                   ":443, unable to enable SSL.";
        }
    }
    return "";
}

sub check_priority {
    my ($value) = (@_);
    &::GetVersionTable();
    if (lsearch(\@::legal_priority, $value) < 0) {
        return "Must be a legal priority value: one of " .
            join(", ", @::legal_priority);
    }
    return "";
}

sub check_severity {
    my ($value) = (@_);
    &::GetVersionTable();
    if (lsearch(\@::legal_severity, $value) < 0) {
        return "Must be a legal severity value: one of " .
            join(", ", @::legal_severity);
    }
    return "";
}

sub check_platform {
    my ($value) = (@_);
    &::GetVersionTable();
    if (lsearch(['', @::legal_platform], $value) < 0) {
        return "Must be empty or a legal platform value: one of " .
            join(", ", @::legal_platform);
    }
    return "";
}

sub check_opsys {
    my ($value) = (@_);
    &::GetVersionTable();
    if (lsearch(['', @::legal_opsys], $value) < 0) {
        return "Must be empty or a legal operating system value: one of " .
            join(", ", @::legal_opsys);
    }
    return "";
}

sub check_shadowdb {
    my ($value) = (@_);
    $value = trim($value);
    if ($value eq "") {
        return "";
    }

    if (!Param('shadowdbhost')) {
        return "You need to specify a host when using a shadow database";
    }

    # Can't test existence of this because ConnectToDatabase uses the param,
    # but we can't set this before testing....
    # This can really only be fixed after we can use the DBI more openly
    return "";
}

sub check_urlbase {
    my ($url) = (@_);
    if ($url !~ m:^http.*/$:) {
        return "must be a legal URL, that starts with http and ends with a slash.";
    }
    return "";
}

sub check_webdotbase {
    my ($value) = (@_);
    $value = trim($value);
    if ($value eq "") {
        return "";
    }
    if($value !~ /^https?:/) {
        if(! -x $value) {
            return "The file path \"$value\" is not a valid executable.  Please specify the complete file path to 'dot' if you intend to generate graphs locally.";
        }
        # Check .htaccess allows access to generated images
        if(-e "$webdotdir/.htaccess") {
            open HTACCESS, "$webdotdir/.htaccess";
            if(! grep(/ \\\.png\$/,<HTACCESS>)) {
                return "Dependency graph images are not accessible.\nAssuming that you have not modified the file, delete $webdotdir/.htaccess and re-run checksetup.pl to rectify.\n";
            }
            close HTACCESS;
        }
    }
    return "";
}

sub check_netmask {
    my ($mask) = @_;
    my $res = check_numeric($mask);
    return $res if $res;
    if ($mask < 0 || $mask > 32) {
        return "an IPv4 netmask must be between 0 and 32 bits";
    }
    # Note that if we changed the netmask from anything apart from 32, then
    # existing logincookies which aren't for a single IP won't work
    # any more. We can't know which ones they are, though, so they'll just
    # take space until they're preiodically cleared, later.

    return "";
}

sub check_user_verify_class {
    # doeditparams traverses the list of params, and for each one it checks,
    # then updates. This means that if one param checker wants to look at 
    # other params, it must be below that other one. So you can't have two 
    # params mutually dependent on each other.
    # This means that if someone clears the LDAP config params after setting
    # the login method as LDAP, we won't notice, but all logins will fail.
    # So don't do that.

    my ($list, $entry) = @_;
    for my $class (split /,\s*/, $list) {
        my $res = check_multi($class, $entry);
        return $res if $res;
        if ($class eq 'DB') {
            # No params
        } elsif ($class eq 'LDAP') {
            eval "require Net::LDAP";
            return "Error requiring Net::LDAP: '$@'" if $@;
            return "LDAP servername is missing" unless Param("LDAPserver");
            return "LDAPBaseDN is empty" unless Param("LDAPBaseDN");
        } else {
                return "Unknown user_verify_class '$class' in check_user_verify_class";
        }
    }
    return "";
}

sub check_languages {
    my @languages = split /[,\s]+/, trim($_[0]);
    if(!scalar(@languages)) {
       return "You need to specify a language tag."
    }
    foreach my $language (@languages) {
       if(   ! -d "$templatedir/$language/custom" 
          && ! -d "$templatedir/$language/default") {
          return "The template directory for $language does not exist";
       }
    }
    return "";
}

sub find_languages {
    my @languages = ();
    opendir(DIR, $templatedir) || return "Can't open 'template' directory: $!";
    foreach my $dir (readdir(DIR)) {
        next unless $dir =~ /^([a-z-]+)$/i;
        my $lang = $1;
        next if($lang =~ /^CVS$/i);
        my $deft_path = File::Spec->catdir('template', $lang, 'default');
        my $cust_path = File::Spec->catdir('template', $lang, 'custom');
        push(@languages, $lang) if(-d $deft_path or -d $cust_path);
    }
    closedir DIR;
    return join(', ', @languages);
}

sub check_mail_delivery_method {
    my $check = check_multi(@_);
    return $check if $check;
    my $mailer = shift;
    if ($mailer eq 'sendmail' && $^O =~ /MSWin32/i) {
        # look for sendmail.exe 
        return "Failed to locate " . SENDMAIL_EXE
            unless -e SENDMAIL_EXE;
    }
    return "";
}

# OK, here are the parameter definitions themselves.
#
# Each definition is a hash with keys:
#
# name    - name of the param
# desc    - description of the param (for editparams.cgi)
# type    - see below
# choices - (optional) see below
# default - default value for the param
# checker - (optional) checking function for validating parameter entry
#           It is called with the value of the param as the first arg and a
#           reference to the param's hash as the second argument
#
# The type value can be one of the following:
#
# t -- A short text entry field (suitable for a single line)
# l -- A long text field (suitable for many lines)
# b -- A boolean value (either 1 or 0)
# m -- A list of values, with many selectable (shows up as a select box)
#      To specify the list of values, make the 'choices' key be an array
#      reference of the valid choices. The 'default' key should be an array
#      reference for the list of selected values (which must appear in the
#      first anonymous array), i.e.:
#       {
#         name => 'multiselect',
#         desc => 'A list of options, choose many',
#         type => 'm',
#         choices => [ 'a', 'b', 'c', 'd' ],
#         default => [ 'a', 'd' ],
#         checker => \&check_multi
#       }
#
#      Here, 'a' and 'd' are the default options, and the user may pick any
#      combination of a, b, c, and d as valid options.
#
#      &check_multi should always be used as the param verification function
#      for list (single and multiple) parameter types.
#
# s -- A list of values, with one selectable (shows up as a select box)
#      To specify the list of values, make the 'choices' key be an array
#      reference of the valid choices. The 'default' key should be one of
#      those values, i.e.:
#       {
#         name => 'singleselect',
#         desc => 'A list of options, choose one',
#         type => 's',
#         choices => [ 'a', 'b', 'c' ],
#         default => 'b',
#         checker => \&check_multi
#       }
#
#      Here, 'b' is the default option, and 'a' and 'c' are other possible
#      options, but only one at a time! 
#
#      &check_multi should always be used as the param verification function
#      for list (single and multiple) parameter types.

# XXXX - would be nice for doeditparams to 'know' about types s and m, and call
# check_multi without it having to be explicitly specified here - bbaetz

@param_list = (
  {
   name => 'maintainer',
   desc => 'The email address of the person who maintains this installation ' .
           'of Bugzilla.',
   type => 't',
   default => 'THE MAINTAINER HAS NOT YET BEEN SET'
  },

  {
   name => 'urlbase',
   desc => 'The URL that is the common initial leading part of all Bugzilla ' .
           'URLs.',
   type => 't',
   default => 'http://you-havent-visited-editparams.cgi-yet/',
   checker => \&check_urlbase
  },

  {
   name => 'sslbase',
   desc => 'The URL that is the common initial leading part of all HTTPS ' .
           '(SSL) Bugzilla URLs.',
   type => 't',
   default => '',
   checker => \&check_sslbase
  },

  {
   name => 'ssl',
   desc => 'Controls when Bugzilla should enforce sessions to use HTTPS by ' .
           'using <tt>sslbase</tt>.',
   type => 's',
   choices => ['never', 'authenticated sessions', 'always'],
   default => 'never'
  },

  {
   name => 'languages' ,
   desc => 'A comma-separated list of RFC 1766 language tags. These ' .
           'identify the languages in which you wish Bugzilla output ' .
           'to be displayed. Note that you must install the appropriate ' .
           'language pack before adding a language to this Param. The ' .
           'language used is the one in this list with the highest ' .
           'q-value in the user\'s Accept-Language header.<br>' .
           'Available languages: ' . find_languages() ,
   type => 't' ,
   default => 'en' ,
   checker => \&check_languages
  },

  {
   name => 'defaultlanguage',
   desc => 'The UI language Bugzilla falls back on if no suitable ' .
           'language is found in the user\'s Accept-Language header.' ,
   type => 't' ,
   default => 'en' ,
   checker => \&check_languages
  },

  {
   name => 'cookiedomain',
   desc => 'The domain for Bugzilla cookies.  Normally blank.  ' .
           'If your website is at "www.foo.com", setting this to ' .
           '".foo.com" will also allow bar.foo.com to access ' .
           'Bugzilla cookies.  This is useful if you have more than ' .
           'one hostname pointing at the same web server, and you ' .
           'want them to share the Bugzilla cookie.',
   type => 't',
   default => ''
  },
  {
   name => 'cookiepath',
   desc => 'Path, relative to your web document root, to which to restrict ' .
           'Bugzilla cookies.  Normally this is the URI portion of your URL ' .
           'base.  Begin with a / (single slash mark).  For instance, if ' .
           'Bugzilla serves from http://www.somedomain.com/bugzilla/, set ' .
           'this parameter to /bugzilla/ .  Setting it to / will allow ' .
           'all sites served by this web server or virtual host to read ' .
           'Bugzilla cookies.',
   type => 't',
   default => '/'
  },

  {
   name => 'timezone',
   desc => 'The timezone that your database server lives in. If set to "", ' .
           'then the timezone won\'t be displayed with the timestamps.',
   type => 't',
   default => '',
  },

  {
   name => 'quip_list_entry_control',
   desc => 'Controls how easily users can add entries to the quip list.' .
           '<ul><li>open - Users may freely add to the quip list, and ' .
           'their entries will immediately be available for viewing.</li>' .
           '<li>moderated - quips can be entered, but need to be approved ' .
           'by an admin before they will be shown</li><li>closed - no new ' .
           'additions to the quips list are allowed.</li></ul>',
   type => 's',
   choices => ['open', 'moderated', 'closed'],
   default => 'open',
   checker => \&check_multi
  },

  {
   name => 'useclassification',
   desc => 'If this is on, Bugzilla will associate each product with a ' .
           'specific classification.  But you must have "editclassification" ' .
           'permissions enabled in order to edit classifications',
   type => 'b',
   default => 0
  },

  {
   name => 'showallproducts',
   desc => 'If this is on and useclassification is set, Bugzilla will add a' .
           '"All" link in the "New Bug" page to list all available products',
   type => 'b',
   default => 0
  },

  {
   name => 'makeproductgroups',
   desc => 'If this is on, Bugzilla will associate a bug group with each ' .
           'product in the database, and use it for querying bugs.',
   type => 'b',
   default => 0
  },

  {
   name => 'useentrygroupdefault',
   desc => 'If this is on, Bugzilla will use product bug groups by default ' .
           'to restrict who can enter bugs. If this is on, users can see ' .
           'any product to which they have entry access in search menus. ' .
           'If this is off, users can see any product to which they have not ' .
           'been excluded by a mandatory restriction.',
   type => 'b',
   default => 0
  },

  {
   name => 'shadowdbhost',
   desc => 'The host the shadow database is on.',
   type => 't',
   default => '',
  },

  {
   name => 'shadowdbport',
   desc => 'The port the shadow database is on. Ignored if ' .
           '<tt>shadowdbhost</tt> is blank. Note: if the host is the local ' .
           'machine, then MySQL will ignore this setting, and you must ' .
           'specify a socket below.',
   type => 't',
   default => '3306',
   checker => \&check_numeric,
  },

  {
   name => 'shadowdbsock',
   desc => 'The socket used to connect to the shadow database, if the host ' .
           'is the local machine. This setting is required because MySQL ' .
           'ignores the port specified by the client and connects using ' .
           'its compiled-in socket path (on unix machines) when connecting ' .
           'from a client to a local server. If you leave this blank, and ' .
           'have the database on localhost, then the <tt>shadowdbport</tt> ' .
           'will be ignored.',
   type => 't',
   default => '',
  },

  # This entry must be _after_ the shadowdb{host,port,sock} settings so that
  # they can be used in the validation here
  {
   name => 'shadowdb',
   desc => 'If non-empty, then this is the name of another database in ' .
           'which Bugzilla will use as a read-only copy of everything. ' .
           'This is done so that long slow read-only operations can be used ' .
           'against this db, and not lock up things for everyone else. This ' .
           'database is on the <tt>shadowdbhost</tt>, and must exist. ' .
           'Bugzilla does not update it, if you use this parameter, then ' .
           'you need to set up replication for your database',
   type => 't',
   default => '',
   checker => \&check_shadowdb
  },

  {
   name => 'LDAPserver',
   desc => 'The name (and optionally port) of your LDAP server. (e.g. ' .
           'ldap.company.com, or ldap.company.com:portnum)',
   type => 't',
   default => ''
  },

  {
   name => 'LDAPbinddn',
   desc => 'If your LDAP server requires that you use a binddn and password ' .
           'instead of binding anonymously, enter it here ' .
           '(e.g. cn=default,cn=user:password). ' .
           'Leave this empty for the normal case of an anonymous bind.',
   type => 't',
   default => ''
  },

  {
   name => 'LDAPBaseDN',
   desc => 'The BaseDN for authenticating users against. (e.g. ' .
           '"ou=People,o=Company")',
   type => 't',
   default => ''
  },

  {
   name => 'LDAPuidattribute',
   desc => 'The name of the attribute containing the user\'s login name.',
   type => 't',
   default => 'uid'
  },

  {
   name => 'LDAPmailattribute',
   desc => 'The name of the attribute of a user in your directory that ' .
           'contains the email address.',
   type => 't',
   default => 'mail'
  },

  {
   name => 'LDAPfilter',
   desc => 'LDAP filter to AND with the <tt>LDAPuidattribute</tt> for ' .
           'filtering the list of valid users.',
   type => 't',
   default => '',
  },

  {
   name => 'auth_env_id',
   desc    => 'Environment variable used by external authentication system ' .
              'to store a unique identifier for each user.  Leave it blank ' .
              'if there isn\'t one or if this method of authentication ' .
              'is not being used.',
   type    => 't',
   default => '',
  },

  {
   name    => 'auth_env_email',
   desc    => 'Environment variable used by external authentication system ' .
              'to store each user\'s email address.  This is a required ' .
              'field for environmental authentication.  Leave it blank ' .
              'if you are not going to use this feature.',
   type    => 't',
   default => '',
  },

  {
   name    => 'auth_env_realname',
   desc    => 'Environment variable used by external authentication system ' .
              'to store the user\'s real name.  Leave it blank if there ' .
              'isn\'t one or if this method of authentication is not being ' .
              'used.',
   type    => 't',
   default => '',
  },

  # XXX in the future:
  #
  # user_verify_class and user_info_class should have choices gathered from
  # whatever sits in their respective directories
  #
  # rather than comma-separated lists, these two should eventually become
  # arrays, but that requires alterations to editparams first

  {
   name => 'user_info_class',
   desc => 'Mechanism(s) to be used for gathering a user\'s login information.
              <add>
            More than one may be selected. If the first one returns nothing,
            the second is tried, and so on.<br />
            The types are:
            <dl>
              <dt>CGI</dt>
              <dd>
                Asks for username and password via CGI form interface.
              </dd>
              <dt>Env</dt>
              <dd>
                Info for a pre-authenticated user is passed in system
                environment variables.
              </dd>
            </dl>',
   type => 's',
   choices => [ 'CGI', 'Env', 'Env,CGI' ],
   default => 'CGI',
   checker => \&check_multi
  },

  {
   name => 'user_verify_class',
   desc => 'Mechanism(s) to be used for verifying (authenticating) information
            gathered by user_info_class.
            More than one may be selected. If the first one cannot find the
            user, the second is tried, and so on.<br />
            The types are:
            <dl>
              <dt>DB</dt>
              <dd>
                Bugzilla\'s built-in authentication. This is the most common
                choice.
              </dd>
              <dt>LDAP</dt>
              <dd>
                LDAP authentication using an LDAP server. This method is
                experimental; please see the Bugzilla documentation for more
                information. Using this method requires additional parameters
                to be set above.
              </dd>
             </dl>',
   type => 's',
   choices => [ 'DB', 'LDAP', 'DB,LDAP', 'LDAP,DB' ],
   default => 'DB',
   checker => \&check_user_verify_class
  },

  {
   name => 'rememberlogin',
   desc => 'Controls management of session cookies
           <ul>
           <li>on - Session cookies never expire (the user has to login only
           once per browser).</li>
           <li>off - Session cookies last until the users session ends (the user
             will have to login in each new browser session).</li>
           <li>defaulton/defaultoff - Default behavior as described
           above, but user can choose whether bugzilla will remember his
           login or not.</li>
           </ul>',
   type => 's',
   choices => ['on', 'defaulton', 'defaultoff', 'off'],
   default => 'on',
   checker => \&check_multi
  },

  {
   name => 'mostfreqthreshold',
   desc => 'The minimum number of duplicates a bug needs to show up on the ' .
           '<a href="duplicates.cgi">most frequently reported bugs page</a>. ' .
           'If you have a large database and this page takes a long time to ' .
           'load, try increasing this number.',
   type => 't',
   default => '2',
   checker => \&check_numeric
  },

  {
   name => 'mybugstemplate',
   desc => 'This is the URL to use to bring up a simple \'all of my bugs\' ' .
           'list for a user.  %userid% will get replaced with the login ' .
           'name of a user.',
   type => 't',
   default => 'buglist.cgi?bug_status=NEW&amp;bug_status=ASSIGNED&amp;bug_status=REOPENED&amp;email1=%userid%&amp;emailtype1=exact&amp;emailassigned_to1=1&amp;emailreporter1=1'
  },

  {
   name => 'shutdownhtml',
   desc => 'If this field is non-empty, then Bugzilla will be completely ' .
           'disabled and this text will be displayed instead of all the ' .
           'Bugzilla pages.',
   type => 'l',
   default => ''
  },

  {
   name => 'mail_delivery_method',
   desc => 'Defines how email is sent, or if it is sent at all.<br><ul>' .
           '<li>\'sendmail\', \'smtp\' and \'qmail\' are all MTAs. ' .
           'You need to install a third-party sendmail replacement if ' .
           'you want to use sendmail on Windows.' .
           '<li>\'testfile\' is useful for debugging: all email is stored' .
           'in data/mailer.testfile instead of being sent. For more ' .
           'information, see the Mail::Mailer manual.</li>' .
           '<li>\'none\' will completely disable email. Bugzilla continues ' .
           'to act as though it is sending mail, but nothing is sent or ' .
           'stored.</li></ul>' ,
   type => 's',
   choices => $^O =~ /MSWin32/i 
                  ? ['smtp', 'testfile', 'sendmail', 'none']
                  : ['sendmail', 'smtp', 'qmail', 'testfile', 'none'],
   default => 'sendmail',
   checker => \&check_mail_delivery_method
  },

  {
   name => 'sendmailnow',
   desc => 'Sites using anything older than version 8.12 of \'sendmail\' ' .
           'can achieve a significant performance increase in the ' .
           'UI -- at the cost of delaying the sending of mail -- by ' .
           'disabling this parameter. Sites using \'sendmail\' 8.12 or ' .
           'higher should leave this on, as they will see no benefit from ' .
           'turning it off. Sites using an MTA other than \'sendmail\' ' .
           '*must* leave it on, or no bug mail will be sent.',
   type => 'b',
   default => 1
  },

  {
   name => 'smtpserver',
   desc => 'The SMTP server address (if using SMTP for mail delivery).',
   type => 't',
   default => 'localhost'
  },

  {
   name => 'passwordmail',
   desc => 'The email that gets sent to people to tell them their password.' .
           'Within this text, %mailaddress% gets replaced by the person\'s ' .
           'email address, %login% gets replaced by the person\'s login ' .
           '(usually the same thing), and %password% gets replaced by their ' .
           'password.  %<i>anythingelse</i>% gets replaced by the ' .
           'definition of that parameter (as defined on this page).',
   type => 'l',
   default => 'From: bugzilla-daemon
To: %mailaddress%
Subject: Your Bugzilla password.

To use the wonders of Bugzilla, you can use the following:

 E-mail address: %login%
       Password: %password%

 To change your password, go to:
 %urlbase%userprefs.cgi
'
  },

  {
   name => 'newchangedmail',
   desc => 'The email that gets sent to people when a bug changes. Within ' .
           'this text, %to% gets replaced with the e-mail address of the ' .
           'person receiving the mail.  %bugid% gets replaced by the bug ' .
           'number.  %diffs% gets replaced with what\'s changed. ' .
           '%neworchanged% is "New:" if this mail is reporting a new bug or ' .
           'empty if changes were made to an existing one. %summary% gets ' .
           'replaced by the summary of this bug. %reasonsheader% is ' .
           'replaced by an abbreviated list of reasons why the user is ' .
           'getting the email, suitable for use in an email header (such ' .
           'as X-Bugzilla-Reason). %reasonsbody% is replaced by text that ' .
           'explains why the user is getting the email in more user ' .
           'friendly text than %reasonsheader%. ' .
           '%threadingmarker% will become either a Message-ID line (for ' .
           'new-bug messages) or a In-Reply-To line (for bug-change ' .
           'messages). ' .
           '%<i>anythingelse</i>% gets ' .
           'replaced by the definition of that parameter (as defined on ' .
           'this page).',
   type => 'l',
   default => 'From: bugzilla-daemon
To: %to%
Subject: [Bug %bugid%] %neworchanged%%summary%
%threadingmarker%
X-Bugzilla-Reason: %reasonsheader%
X-Bugzilla-Product: %product%
X-Bugzilla-Component: %component%

%urlbase%show_bug.cgi?id=%bugid%

%diffs%

--%space%
Configure bugmail: %urlbase%userprefs.cgi?tab=email
%reasonsbody%'
  },

  {
   name => 'whinedays',
   desc => q{The number of days that we'll let a bug sit untouched in a NEW
             state before our cronjob will whine at the owner.<br>
             Set to 0 to disable whining.},
   type => 't',
   default => 7,
   checker => \&check_numeric
  },

  {
   name => 'whinemail',
   desc => 'The email that gets sent to anyone who has a NEW or REOPENED ' .
           'bug that hasn\'t been touched for more than <b>whinedays</b>.  ' .
           'Within this text, %email% gets replaced by the offender\'s ' .
           'email address. %userid% gets replaced by the offender\'s ' .
           'bugzilla login (which, in most installations, is the same as ' .
           'the email address.) %<i>anythingelse</i>% gets replaced by the ' .
           'definition of that parameter (as defined on this page).<p> It ' .
           'is a good idea to make sure this message has a valid From: ' .
           'address, so that if the mail bounces, a real person can know '.
           'that there are bugs assigned to an invalid address.',
   type => 'l',
   default => 'From: %maintainer%
To: %email%
Subject: Your Bugzilla buglist needs attention.

[This e-mail has been automatically generated.]

You have one or more bugs assigned to you in the Bugzilla 
bugsystem (%urlbase%) that require
attention.

All of these bugs are in the NEW or REOPENED state, and have not
been touched in %whinedays% days or more.  You need to take a look
at them, and decide on an initial action.

Generally, this means one of three things:

(1) You decide this bug is really quick to deal with (like, it\'s INVALID),
    and so you get rid of it immediately.
(2) You decide the bug doesn\'t belong to you, and you reassign it to someone
    else.  (Hint: if you don\'t know who to reassign it to, make sure that
    the Component field seems reasonable, and then use the "Reassign bug to
    default assignee of selected component" option.)
(3) You decide the bug belongs to you, but you can\'t solve it this moment.
    Just use the "Accept bug" command.

To get a list of all NEW/REOPENED bugs, you can use this URL (bookmark
it if you like!):

 %urlbase%buglist.cgi?bug_status=NEW&bug_status=REOPENED&assigned_to=%userid%

Or, you can use the general query page, at
%urlbase%query.cgi

Appended below are the individual URLs to get to all of your NEW bugs that
haven\'t been touched for a week or more.

You will get this message once a day until you\'ve dealt with these bugs!

'
  },

  {
   name => 'defaultquery',
   desc => 'This is the default query that initially comes up when you ' .
           'access the advanced query page.  It\'s in URL parameter ' .
           'format, which makes it hard to read.  Sorry!',
   type => 't',
   default => 'bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&emailassigned_to1=1&emailassigned_to2=1&emailreporter2=1&emailcc2=1&emailqa_contact2=1&order=Importance&long_desc_type=substring'
  },

  {
   name => 'letsubmitterchoosepriority',
   desc => 'If this is on, then people submitting bugs can choose an ' .
           'initial priority for that bug.  If off, then all bugs initially ' .
           'have the default priority selected below.',
   type => 'b',
   default => 1
  },

  {
   name => 'defaultpriority',
   desc => 'This is the priority that newly entered bugs are set to.',
   type => 't',
   default => 'P2',
   checker => \&check_priority
  },

  {
   name => 'defaultseverity',
   desc => 'This is the severity that newly entered bugs are set to.',
   type => 't',
   default => 'normal',
   checker => \&check_severity
  },

  {
    name => 'defaultplatform',
    desc => 'This is the platform that is preselected on the bug '.
            'entry form.<br>'.
            'You can leave this empty: '.
            'Bugzilla will then use the platform that the browser '.
            'reports to be running on as the default.',
    type => 't',
    default => '',
    checker => \&check_platform
  },

  {
    name => 'defaultopsys',
    desc => 'This is the operating system that is preselected on the bug '.
            'entry form.<br>'.
            'You can leave this empty: '.
            'Bugzilla will then use the operating system that the browser '.
            'reports to be running on as the default.',
    type => 't',
    default => '',
    checker => \&check_opsys
  },

  {
   name => 'usetargetmilestone',
   desc => 'Do you wish to use the Target Milestone field?',
   type => 'b',
   default => 0
  },

  {
   name => 'letsubmitterchoosemilestone',
   desc => 'If this is on, then people submitting bugs can choose the ' .
           'Target Milestone for that bug.  If off, then all bugs initially ' .
           'have the default milestone for the product being filed in.',
   type => 'b',
   default => 1
  },

  {
   name => 'musthavemilestoneonaccept',
   desc => 'If you are using Target Milestone, do you want to require that ' .
           'the milestone be set in order for a user to ACCEPT a bug?',
   type => 'b',
   default => 0
  },

  {
   name => 'useqacontact',
   desc => 'Do you wish to use the QA Contact field?',
   type => 'b',
   default => 0
  },

  {
   name => 'usestatuswhiteboard',
   desc => 'Do you wish to use the Status Whiteboard field?',
   type => 'b',
   default => 0
  },

  {
   name => 'usevotes',
   desc => 'Do you wish to allow users to vote for bugs? Note that in order ' .
           'for this to be effective, you will have to change the maximum ' .
           'votes allowed in a product to be non-zero in ' .
           '<a href="editproducts.cgi">the product edit page</a>.',
   type => 'b',
   default => 1
  },

  {
   name => 'usebugaliases',
   desc => 'Do you wish to use bug aliases, which allow you to assign bugs ' .
           'an easy-to-remember name by which you can refer to them?',
   type => 'b',
   default => 0
  },

  {
   name => 'usevisibilitygroups',
   desc => 'Do you wish to restrict visibility of users to members of ' .
           'specific groups?',
   type => 'b',
   default => 0
  },

  {
   name => 'webdotbase',
   desc => 'It is possible to show graphs of dependent bugs. You may set ' .
           'this parameter to any of the following:
   <ul>
   <li>A complete file path to \'dot\' (part of <a
       href="http://www.graphviz.org">GraphViz</a>) will generate the graphs
   locally.</li>
   <li>A URL prefix pointing to an installation of the <a
   href="http://www.research.att.com/~north/cgi-bin/webdot.cgi">webdot
   package</a> will generate the graphs remotely.</li>
   <li>A blank value will disable dependency graphing.</li>
   </ul>
   The default value is a publicly-accessible webdot server. If you change
   this value, make certain that the webdot server can read files from your
   webdot directory. On Apache you do this by editing the .htaccess file,
   for other systems the needed measures may vary. You can run checksetup.pl
   to recreate the .htaccess file if it has been lost.',
   type => 't',
   default => 'http://www.research.att.com/~north/cgi-bin/webdot.cgi/%urlbase%',
   checker => \&check_webdotbase
  },

  {
   name => 'emailregexp',
   desc => 'This defines the regexp to use for legal email addresses. The ' .
           'default tries to match fully qualified email addresses. Another ' .
           'popular value to put here is <tt>^[^@]+$</tt>, which means ' .
           '"local usernames, no @ allowed."',
   type => 't',
   default => q:^[\\w\\.\\+\\-=]+@[\\w\\.\\-]+\\.[\\w\\-]+$:,
   checker => \&check_regexp
  },

  {
   name => 'emailregexpdesc',
   desc => 'This describes in English words what kinds of legal addresses ' .
           'are allowed by the <tt>emailregexp</tt> param.',
   type => 'l',
   default => 'A legal address must contain exactly one \'@\', and at least ' .
              'one \'.\' after the @.'
  },

  {
   name => 'emailsuffix',
   desc => 'This is a string to append to any email addresses when actually ' .
           'sending mail to that address.  It is useful if you have changed ' .
           'the <tt>emailregexp</tt> param to only allow local usernames, ' .
           'but you want the mail to be delivered to username@my.local.hostname.',
   type => 't',
   default => ''
  },

  {
   name => 'createemailregexp',
   desc => 'This defines the regexp to use for email addresses that are ' .
           'permitted to self-register using a "New Account" feature. The ' .
           'default (.*) permits any account matching the emailregexp ' .
           'to be created.  If this parameter is left blank, no users ' .
           'will be permitted to create their own accounts and all accounts ' .
           'will have to be created by an administrator',
   type => 't',
   default => q:.*:,
   checker => \&check_regexp
  },

  {
   name => 'voteremovedmail',
   desc => 'This is a mail message to send to anyone who gets a vote removed ' .
           'from a bug for any reason.  %to% gets replaced by the person who ' .
           'used to be voting for this bug.  %bugid% gets replaced by the ' .
           'bug number. %reason% gets replaced by a short reason describing ' .
           'why the vote(s) were removed. %votesremoved%, %votesold% and ' .
           '%votesnew% is the number of votes removed, before and after ' .
           'respectively. %votesremovedtext%, %votesoldtext% and ' .
           '%votesnewtext% are these as sentences, e.g. "You had 2 votes on ' .
           'this bug."  %count% is also supported for backwards ' .
           'compatibility. %<i>anythingelse</i>% gets replaced by the ' .
           'definition of that parameter (as defined on this page).',
   type => 'l',
   default => 'From: bugzilla-daemon
To: %to%
Subject: [Bug %bugid%] Some or all of your votes have been removed.

Some or all of your votes have been removed from bug %bugid%.

%votesoldtext%

%votesnewtext%

Reason: %reason%

%urlbase%show_bug.cgi?id=%bugid%
'
  },

  {
   name => 'allowbugdeletion',
   desc => 'The pages to edit products and components and versions can delete ' .
           'all associated bugs when you delete a product (or component or ' .
           'version).  Since that is a pretty scary idea, you have to turn on ' .
           'this option before any such deletions will ever happen.',
   type => 'b',
   default => 0
  },

  {
   name => 'allowemailchange',
   desc => 'Users can change their own email address through the preferences. ' .
           'Note that the change is validated by emailing both addresses, so ' .
           'switching this option on will not let users use an invalid address.',
   type => 'b',
   default => 0
  },

  {
   name => 'allowuserdeletion',
   desc => q{The user editing pages are capable of letting you delete user
             accounts.
             Bugzilla will issue a warning in case you'd run into
             inconsistencies when you're about to do so,
             but such deletions remain kinda scary.
             So, you have to turn on this option before any such deletions
             will ever happen.},
   type => 'b',
   default => 0
  },

  {
   name => 'browserbugmessage',
   desc => 'If bugzilla gets unexpected data from the browser, in addition to ' .
           'displaying the cause of the problem, it will output this HTML as ' .
           'well.',
   type => 'l',
   default => 'this may indicate a bug in your browser.'
  },

  {
   name => 'commentoncreate',
   desc => 'If this option is on, the user needs to enter a description ' .
           'when entering a new bug',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonaccept',
   desc => 'If this option is on, the user needs to enter a short comment if ' .
           'he accepts the bug',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonclearresolution',
   desc => 'If this option is on, the user needs to enter a short comment if ' .
           'the bug\'s resolution is cleared',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonconfirm',
   desc => 'If this option is on, the user needs to enter a short comment ' .
           'when confirming a bug',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonresolve',
   desc => 'If this option is on, the user needs to enter a short comment if ' .
           'the bug is resolved',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonreassign',
   desc => 'If this option is on, the user needs to enter a short comment if ' .
           'the bug is reassigned',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonreassignbycomponent',
   desc => 'If this option is on, the user needs to enter a short comment if ' .
           'the bug is reassigned by component',
   type => 'b',
   default => 0
  },
  {
   name => 'commentonreopen',
   desc => 'If this option is on, the user needs to enter a short comment if ' .
           'the bug is reopened',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonverify',
   desc => 'If this option is on, the user needs to enter a short comment if ' .
           'the bug is verified',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonclose',
   desc => 'If this option is on, the user needs to enter a short comment if ' .
           'the bug is closed',
   type => 'b',
   default => 0
  },

  {
   name => 'commentonduplicate',
   desc => 'If this option is on, the user needs to enter a short comment ' .
           'if the bug is marked as duplicate',
   type => 'b',
   default => 0
  },

  {
   name => 'supportwatchers',
   desc => 'Support one user watching (ie getting copies of all related ' .
           'email about) another\'s bugs.  Useful for people going on ' .
           'vacation, and QA folks watching particular developers\' bugs',
   type => 'b',
   default => 0
  },

  {
   name => 'move-enabled',
   desc => 'If this is on, Bugzilla will allow certain people to move bugs ' .
           'to the defined database.',
   type => 'b',
   default => 0
  },

  {
   name => 'move-button-text',
   desc => 'The text written on the Move button. Explain where the bug is ' .
           'being moved to.',
   type => 't',
   default => 'Move To Bugscape'
  },

  {
   name => 'move-to-url',
   desc => 'The URL of the database we allow some of our bugs to be moved to.',
   type => 't',
   default => ''
  },

  {
   name => 'move-to-address',
   desc => 'To move bugs, an email is sent to the target database. This is ' .
           'the email address that database uses to listen for incoming bugs.',
   type => 't',
   default => 'bugzilla-import'
  },

  {
   name => 'moved-from-address',
   desc => 'To move bugs, an email is sent to the target database. This is ' .
           'the email address from which this mail, and error messages are ' .
           'sent.',
   type => 't',
   default => 'bugzilla-admin'
  },

  {
   name => 'movers',
   desc => 'A list of people with permission to move bugs and reopen moved ' .
           'bugs (in case the move operation fails).',
   type => 't',
   default => ''
  },

  {
   name => 'moved-default-product',
   desc => 'Bugs moved from other databases to here are assigned to this ' .
           'product.',
   type => 't',
   default => ''
  },

  {
   name => 'moved-default-component',
   desc => 'Bugs moved from other databases to here are assigned to this ' .
           'component.',
   type => 't',
   default => ''
  },

  # The maximum size (in bytes) for patches and non-patch attachments.
  # The default limit is 1000KB, which is 24KB less than mysql's default
  # maximum packet size (which determines how much data can be sent in a
  # single mysql packet and thus how much data can be inserted into the
  # database) to provide breathing space for the data in other fields of
  # the attachment record as well as any mysql packet overhead (I don't
  # know of any, but I suspect there may be some.)

  {
   name => 'maxpatchsize',
   desc => 'The maximum size (in kilobytes) of patches.  Bugzilla will not ' .
           'accept patches greater than this number of kilobytes in size.' .
           'To accept patches of any size (subject to the limitations of ' .
           'your server software), set this value to zero.',
   type => 't',
   default => '1000',
   checker => \&check_numeric
  },

  {
   name => 'maxattachmentsize',
   desc => 'The maximum size (in kilobytes) of non-patch attachments. ' .
           'Bugzilla will not accept attachments greater than this number' .
           'of kilobytes in size.  To accept attachments of any size ' .
           '(subject to the limitations of your server software), set this ' .
           'value to zero.',
   type => 't',
   default => '1000',
   checker => \&check_numeric
  },

  {
   name => 'maxlocalattachment',
   desc => 'The maximum size (in Megabytes) of attachments identified by ' .
           'the user as "Big Files" to be stored locally on the webserver. ' .
           'If set to zero, attachments will never be kept on the local ' .
           'filesystem.',
   type => 't',
   default => '0',
   checker => \&check_numeric
  },

  {
   name => 'chartgroup',
   desc => 'The name of the group of users who can use the "New Charts" ' .
           'feature. Administrators should ensure that the public categories ' .
           'and series definitions do not divulge confidential information ' .
           'before enabling this for an untrusted population. If left blank, ' .
           'no users will be able to use New Charts.',
   type => 't',
   default => 'editbugs'
  },
  
  {
   name => 'insidergroup',
   desc => 'The name of the group of users who can see/change private ' .
           'comments and attachments.',
   type => 't',
   default => ''
  },

  {
   name => 'timetrackinggroup',
   desc => 'The name of the group of users who can see/change time tracking ' .
           'information.',
   type => 't',
   default => ''
  },
  
  {
   name => 'loginnetmask',
   desc => 'The number of bits for the netmask used if a user chooses to ' .
           'allow a login to be valid for more than a single IP. Setting ' .
           'this to 32 disables this feature.<br>' .
           'Note that enabling this may decrease the security of your system.',
   type => 't',
   default => '32',
   checker => \&check_netmask
  },

  {
   name => 'requirelogin',
   desc => 'If this option is set, all access to the system beyond the ' .
           ' front page will require a login. No anonymous users will ' .
           ' be permitted.',
   type => 'b',
   default => '0'
  },

  {
   name => 'usemenuforusers',
   desc => 'If this option is set, Bugzilla will offer you a list' .
           ' to select from (instead of a text entry field) where a user' .
           ' needs to be selected.  This option should not be enabled on' .
           ' sites where there are a large number of users.',
   type => 'b',
   default => '0'
  },

  {
   name => 'usermatchmode',
   desc => 'Allow match strings to be entered for user names when entering ' .
           'and editing bugs.  <p>' .
           '"off" disables matching,<br> ' .
           '"wildcard" allows only wildcards,<br> ' .
           'and "search" allows both wildcards and substring (freetext) ' .
           'matches.',
   type => 's',
   choices => ['off', 'wildcard', 'search'],
   default => 'off'
  },

  {
   name    => 'maxusermatches',
   desc    => 'Search for no more than this many matches.  <br>'.
              'If set to "1", no users will be displayed on ambiguous matches.  '.
              'This is useful for user privacy purposes.  <br>'.
              'A value of zero means no limit.',
   type    => 't',
   default => '1000',
   checker => \&check_numeric
  },

  {
   name    => 'confirmuniqueusermatch',
   desc    => 'Whether a confirmation screen should be displayed when only ' .
               'one user matches a search entry',
   type    => 'b',
   default => 1,
  },

# Added for Patch Viewer stuff (attachment.cgi?action=diff)
  {
   name    => 'cvsroot',
   desc    => 'The <a href="http://www.cvshome.org">CVS</a> root that most ' .
              'users of your system will be using for "cvs diff".  Used in ' .
              'Patch Viewer ("Diff" option on patches) to figure out where ' .
              'patches are rooted even if users did the "cvs diff" from ' .
              'different places in the directory structure.  (NOTE: if your ' .
              'CVS repository is remote and requires a password, you must ' .
              'either ensure the Bugzilla user has done a "cvs login" or ' .
              'specify the password ' .
              '<a href="http://www.cvshome.org/docs/manual/cvs_2.html#SEC26">as ' .
              'part of the CVS root.</a>)  Leave this blank if you have no ' .
              'CVS repository.',
   type    => 't',
   default => '',
  },

  {
   name    => 'cvsroot_get',
   desc    => 'The CVS root Bugzilla will be using to get patches from.  ' .
              'Some installations may want to mirror their CVS repository on ' .
              'the Bugzilla server or even have it on that same server, and ' .
              'thus the repository can be the local file system (and much ' .
              'faster).  Make this the same as cvsroot if you don\'t ' .
              'understand what this is (if cvsroot is blank, make this blank ' .
              'too).',
   type    => 't',
   default => '',
  },

  {
   name    => 'bonsai_url',
   desc    => 'The URL to a ' .
              '<a href="http://www.mozilla.org/bonsai.html">Bonsai</a> ' .
              'server containing information about your CVS repository.  ' .
              'Patch Viewer will use this information to create links to ' .
              'bonsai\'s blame for each section of a patch (it will append ' .
              '"/cvsblame.cgi?..." to this url).  Leave this blank if you ' .
              'don\'t understand what this is.',
   type    => 't',
   default => ''
  },

  {
   name    => 'lxr_url',
   desc    => 'The URL to an ' .
              '<a href="http://sourceforge.net/projects/lxr">LXR</a> server ' .
              'that indexes your CVS repository.  Patch Viewer will use this ' .
              'information to create links to LXR for each file in a patch.  ' .
              'Leave this blank if you don\'t understand what this is.',
   type    => 't',
   default => ''
  },

  {
   name    => 'lxr_root',
   desc    => 'Some LXR installations do not index the CVS repository from ' .
              'the root--' .
              '<a href="http://lxr.mozilla.org/mozilla">Mozilla\'s</a>, for ' .
              'example, starts indexing under <code>mozilla/</code>.  This ' .
              'means URLs are relative to that extra path under the root.  ' .
              'Enter this if you have a similar situation.  Leave it blank ' .
              'if you don\'t know what this is.',
   type    => 't',
   default => '',
  },

  {
   name    => 'noresolveonopenblockers',
   desc    => 'Don\'t allow bugs to be resolved as fixed if they have unresolved dependencies.',
   type    => 'b',
   default => 0,
  },
  
);
1;

