#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use lib qw(/var/www/html /var/www/html/lib); # WEBKIT_CHANGES
package Bugzilla::ModPerl;

use 5.10.1;
use strict;
use warnings;

# This sets up our libpath without having to specify it in the mod_perl
# configuration.
use File::Basename;
use lib dirname(__FILE__);
use Bugzilla::Constants ();
use lib Bugzilla::Constants::bz_locations()->{'ext_libpath'};

# If you have an Apache2::Status handler in your Apache configuration,
# you need to load Apache2::Status *here*, so that any later-loaded modules
# can report information to Apache2::Status.
#use Apache2::Status ();

# We don't want to import anything into the global scope during
# startup, so we always specify () after using any module in this
# file.

use Apache2::Log ();
use Apache2::ServerUtil;
use ModPerl::RegistryLoader ();
use File::Basename ();
use DateTime ();

# This loads most of our modules.
use Bugzilla ();
# Loading Bugzilla.pm doesn't load this, though, and we want it preloaded.
use Bugzilla::BugMail ();
use Bugzilla::CGI ();
use Bugzilla::Extension ();
use Bugzilla::Install::Requirements ();
use Bugzilla::Util ();
use Bugzilla::RNG ();

# Make warnings go to the virtual host's log and not the main
# server log.
BEGIN { *CORE::GLOBAL::warn = \&Apache2::ServerRec::warn; }

# Pre-compile the CGI.pm methods that we're going to use.
Bugzilla::CGI->compile(qw(:cgi :push));

use Apache2::SizeLimit;
# This means that every httpd child will die after processing
# a CGI if it is taking up more than 45MB of RAM all by itself,
# not counting RAM it is sharing with the other httpd processes.
#if WEBKIT_CHANGES
# bugs.webkit.org children are normally about 400MB.
$Apache2::SizeLimit::MAX_UNSHARED_SIZE = 800000;
#endif // WEBKIT_CHANGES

my $cgi_path = Bugzilla::Constants::bz_locations()->{'cgi_path'};

# Set up the configuration for the web server
my $server = Apache2::ServerUtil->server;
my $conf = <<EOT;
# Make sure each httpd child receives a different random seed (bug 476622).
# Bugzilla::RNG has one srand that needs to be called for
# every process, and Perl has another. (Various Perl modules still use
# the built-in rand(), even though we never use it in Bugzilla itself,
# so we need to srand() both of them.)
PerlChildInitHandler "sub { Bugzilla::RNG::srand(); srand(); }"
<Directory "$cgi_path">
    AddHandler perl-script .cgi
    # No need to PerlModule these because they're already defined in mod_perl.pl
    PerlResponseHandler Bugzilla::ModPerl::ResponseHandler
    PerlCleanupHandler  Apache2::SizeLimit Bugzilla::ModPerl::CleanupHandler
    PerlOptions +ParseHeaders
    Options +ExecCGI
    AllowOverride All
    DirectoryIndex index.cgi index.html
</Directory>
EOT

$server->add_config([split("\n", $conf)]);

# Pre-load all extensions
$Bugzilla::extension_packages = Bugzilla::Extension->load_all();

# Have ModPerl::RegistryLoader pre-compile all CGI scripts.
my $rl = new ModPerl::RegistryLoader();
# If we try to do this in "new" it fails because it looks for a 
# Bugzilla/ModPerl/ResponseHandler.pm
$rl->{package} = 'Bugzilla::ModPerl::ResponseHandler';
my $feature_files = Bugzilla::Install::Requirements::map_files_to_features();

# Prevent "use lib" from doing anything when the .cgi files are compiled.
# This is important to prevent the current directory from getting into
# @INC and messing things up. (See bug 630750.)
no warnings 'redefine';
local *lib::import = sub {};
use warnings;

foreach my $file (glob "$cgi_path/*.cgi") {
    my $base_filename = File::Basename::basename($file);
    if (my $feature = $feature_files->{$base_filename}) {
        next if !Bugzilla->feature($feature);
    }
    Bugzilla::Util::trick_taint($file);
    $rl->handler($file, $file);
}

package Bugzilla::ModPerl::ResponseHandler;

use 5.10.1;
use strict;
use warnings;

use parent qw(ModPerl::Registry);
use Bugzilla;
use Bugzilla::Constants qw(USAGE_MODE_REST);

sub handler : method {
    my $class = shift;

    # $0 is broken under mod_perl before 2.0.2, so we have to set it
    # here explicitly or init_page's shutdownhtml code won't work right.
    $0 = $ENV{'SCRIPT_FILENAME'};

    # Prevent "use lib" from modifying @INC in the case where a .cgi file
    # is being automatically recompiled by mod_perl when Apache is
    # running. (This happens if a file changes while Apache is already
    # running.)
    no warnings 'redefine';
    local *lib::import = sub {};
    use warnings;

    Bugzilla::init_page();
    my $result = $class->SUPER::handler(@_);

    # When returning data from the REST api we must only return 200 or 304,
    # which tells Apache not to append its error html documents to the
    # response.
    return Bugzilla->usage_mode == USAGE_MODE_REST && $result != 304
           ? Apache2::Const::OK
           : $result;
}


package Bugzilla::ModPerl::CleanupHandler;

use 5.10.1;
use strict;
use warnings;

use Apache2::Const -compile => qw(OK);

sub handler {
    my $r = shift;

    Bugzilla::_cleanup();
    # Sometimes mod_perl doesn't properly call DESTROY on all
    # the objects in pnotes()
    foreach my $key (keys %{$r->pnotes}) {
        delete $r->pnotes->{$key};
    }

    return Apache2::Const::OK;
}

1;
