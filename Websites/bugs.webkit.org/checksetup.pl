#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This file has detailed POD docs, do "perldoc checksetup.pl" to see them.

######################################################################
# Initialization
######################################################################

use 5.10.1;
use strict;
use warnings;

use File::Basename;
BEGIN { chdir dirname($0); }
use lib qw(. lib);

use Getopt::Long qw(:config bundling);
use Pod::Usage;
use Safe;

use Bugzilla::Constants;
use Bugzilla::Install::Requirements;
use Bugzilla::Install::Util qw(install_string get_version_and_os 
                               init_console success);

######################################################################
# Live Code
######################################################################

# Do not run checksetup.pl from the web browser.
Bugzilla::Install::Util::no_checksetup_from_cgi() if $ENV{'SERVER_SOFTWARE'};

# When we're running at the command line, we need to pick the right
# language before ever displaying any string.
init_console();

my %switch;
GetOptions(\%switch, 'help|h|?', 'check-modules', 'no-templates|t',
                     'verbose|v|no-silent', 'make-admin=s', 
                     'reset-password=s', 'version|V');

# Print the help message if that switch was selected.
pod2usage({-verbose => 1, -exitval => 1}) if $switch{'help'};

# Read in the "answers" file if it exists, for running in 
# non-interactive mode.
my $answers_file = $ARGV[0];
my $silent = $answers_file && !$switch{'verbose'};

print(install_string('header', get_version_and_os()) . "\n") unless $silent;
exit 0 if $switch{'version'};
# Check required --MODULES--
my $module_results = check_requirements(!$silent);
Bugzilla::Install::Requirements::print_module_instructions(
    $module_results, !$silent);
exit 1 if !$module_results->{pass};
# Break out if checking the modules is all we have been asked to do.
exit 0 if $switch{'check-modules'};

###########################################################################
# Load Bugzilla Modules
###########################################################################

# It's never safe to "use" a Bugzilla module in checksetup. If a module
# prerequisite is missing, and you "use" a module that requires it,
# then instead of our nice normal checksetup message, the user would
# get a cryptic perl error about the missing module.

require Bugzilla;
require Bugzilla::User;

require Bugzilla::Util;
import Bugzilla::Util qw(get_text);

require Bugzilla::Config;
import Bugzilla::Config qw(:admin);

require Bugzilla::Install::Localconfig;
import Bugzilla::Install::Localconfig qw(update_localconfig);

require Bugzilla::Install::Filesystem;
import Bugzilla::Install::Filesystem qw(update_filesystem create_htaccess
                                        fix_all_file_permissions);
require Bugzilla::Install::DB;
require Bugzilla::DB;
require Bugzilla::Template;
require Bugzilla::Field;
require Bugzilla::Install;

Bugzilla->installation_mode(INSTALLATION_MODE_NON_INTERACTIVE) if $answers_file;
Bugzilla->installation_answers($answers_file);

###########################################################################
# Check and update --LOCAL-- configuration
###########################################################################

print "Reading " .  bz_locations()->{'localconfig'} . "...\n" unless $silent;
update_localconfig({ output => !$silent });
my $lc_hash = Bugzilla->localconfig;

###########################################################################
# Check --DATABASE-- setup
###########################################################################

# At this point, localconfig is defined and is readable. So we know
# everything we need to create the DB. We have to create it early,
# because some data required to populate data/params.json is stored in the DB.

Bugzilla::DB::bz_check_requirements(!$silent);
Bugzilla::DB::bz_create_database() if $lc_hash->{'db_check'};

# now get a handle to the database:
my $dbh = Bugzilla->dbh;
# Create the tables, and do any database-specific schema changes.
$dbh->bz_setup_database();
# Populate the tables that hold the values for the <select> fields.
$dbh->bz_populate_enum_tables();

###########################################################################
# Check --DATA-- directory
###########################################################################

update_filesystem({ index_html => $lc_hash->{'index_html'} });
create_htaccess() if $lc_hash->{'create_htaccess'};

# Remove parameters from the params file that no longer exist in Bugzilla,
# and set the defaults for new ones
my %old_params = update_params();

###########################################################################
# Pre-compile --TEMPLATE-- code
###########################################################################

Bugzilla::Template::precompile_templates(!$silent)
    unless $switch{'no-templates'};

###########################################################################
# Set proper rights (--CHMOD--)
###########################################################################

fix_all_file_permissions(!$silent);

###########################################################################
# Check GraphViz setup
###########################################################################

# If we are using a local 'dot' binary, verify the specified binary exists
# and that the generated images are accessible.
check_graphviz(!$silent) if Bugzilla->params->{'webdotbase'};

###########################################################################
# Changes to the fielddefs --TABLE--
###########################################################################

# Using Bugzilla::Field's create() or update() depends on the
# fielddefs table having a modern definition. So, we have to make
# these particular schema changes before we make any other schema changes.
Bugzilla::Install::DB::update_fielddefs_definition();

Bugzilla::Field::populate_field_definitions();

###########################################################################
# Update the tables to the current definition --TABLE--
###########################################################################

Bugzilla::Install::DB::update_table_definitions(\%old_params);
Bugzilla::Install::init_workflow();

###########################################################################
# Bugzilla uses --GROUPS-- to assign various rights to its users.
###########################################################################

Bugzilla::Install::update_system_groups();

# "Log In" as the fake superuser who can do everything.
Bugzilla->set_user(Bugzilla::User->super_user);

###########################################################################
# Create --SETTINGS-- users can adjust
###########################################################################

Bugzilla::Install::update_settings();

###########################################################################
# Create Administrator  --ADMIN--
###########################################################################

Bugzilla::Install::make_admin($switch{'make-admin'}) if $switch{'make-admin'};
Bugzilla::Install::create_admin();

Bugzilla::Install::reset_password($switch{'reset-password'})
    if $switch{'reset-password'};

###########################################################################
# Create default Product
###########################################################################

Bugzilla::Install::create_default_product();

Bugzilla::Hook::process('install_before_final_checks', { silent => $silent });

###########################################################################
# Final checks
###########################################################################

# Clear all keys from Memcached
Bugzilla->memcached->clear_all();

# Check if the default parameter for urlbase is still set, and if so, give
# notification that they should go and visit editparams.cgi 
if (Bugzilla->params->{'urlbase'} eq '') {
    print "\n" . get_text('install_urlbase_default') . "\n"
        unless $silent;
}
if (!$silent) {
    success(get_text('install_success'));
}

__END__

=head1 NAME

checksetup.pl - A do-it-all upgrade and installation script for Bugzilla.

=head1 SYNOPSIS

 ./checksetup.pl [--help|--check-modules|--version]
 ./checksetup.pl [SCRIPT [--verbose]] [--no-templates|-t]
                 [--make-admin=user@domain.com]
                 [--reset-password=user@domain.com]

=head1 OPTIONS

=over

=item F<SCRIPT>

Name of script to drive non-interactive mode. This script should
define an C<%answer> hash whose keys are variable names and the
values answers to all the questions checksetup.pl asks. For details
on the format of this script, do C<perldoc checksetup.pl> and look for
the L</"RUNNING CHECKSETUP NON-INTERACTIVELY"> section.

=item B<--help>

Display this help text

=item B<--check-modules>

Only check for correct module dependencies and quit afterward.

=item B<--make-admin>=username@domain.com

Makes the specified user into a Bugzilla administrator. This is
in case you accidentally lock yourself out of the Bugzilla administrative
interface.

=item B<--reset-password>=user@domain.com

Resets the specified user's password. checksetup.pl will prompt you to 
enter a new password for the user.

=item B<--no-templates> (B<-t>)

Don't compile the templates at all. Existing compiled templates will
remain; missing compiled templates will not be created. (Used primarily
by developers to speed up checksetup.) Use this switch at your own risk.

=item B<--verbose>

Output results of SCRIPT being processed.

=item B<--version>

Display the version of Bugzilla, Perl, and some info about the
system that Bugzilla is being installed on, and then exit.

=back

=head1 DESCRIPTION

Hey, what's this?

F<checksetup.pl> is a script that is supposed to run during
installation time and also after every upgrade.

The goal of this script is to make the installation even easier.
It does this by doing things for you as well as testing for problems
in advance.

You can run the script whenever you like. You MUST run it after
you update Bugzilla, because it will then update your SQL table
definitions to resync them with the code.

You can see all the details of what the script does at
L</How Checksetup Works>.

=head1 MODIFYING CHECKSETUP

There should be no need for Bugzilla Administrators to modify
this script; all user-configurable stuff has been moved
into a local configuration file called F<localconfig>. When that file
in changed and F<checksetup.pl> is run, then the user's changes
will be reflected back into the database.

However, developers often need to modify the installation process.
This section explains how F<checksetup.pl> works, so that you
know the right part to modify.

=head2 How Checksetup Works

F<checksetup.pl> runs through several stages during installation:

=over

=item 1

Checks if the required and optional perl modules are installed,
using L<Bugzilla::Install::Requirements/check_requirements>.

=item 2

Creates or updates the F<localconfig> file, using
L<Bugzilla::Install::Localconfig/update_localconfig>.

=item 3

Checks the DBD and database version, using
L<Bugzilla::DB/bz_check_requirements>.

=item 4

Creates the Bugzilla database if it doesn't exist, using
L<Bugzilla::DB/bz_create_database>.

=item 5

Creates all of the tables in the Bugzilla database, using
L<Bugzilla::DB/bz_setup_database>.

Note that all the table definitions are stored in
L<Bugzilla::DB::Schema/ABSTRACT_SCHEMA>.

=item 6

Puts the values into the enum tables (like C<resolution>, C<bug_status>,
etc.) using L<Bugzilla::DB/bz_populate_enum_tables>.

=item 7

Creates any files that Bugzilla needs but doesn't ship with, using
L<Bugzilla::Install::Filesystem/update_filesystem>.

=item 8

Creates the F<.htaccess> files if you haven't specified not to
in F<localconfig>. It does this with
L<Bugzilla::Install::Filesystem/create_htaccess>.

=item 9

Updates the system parameters (stored in F<data/params.json>), using
L<Bugzilla::Config/update_params>.

=item 10

Pre-compiles all templates, to improve the speed of Bugzilla.
It uses L<Bugzilla::Template/precompile_templates> to do this.

=item 11

Fixes all file permissions to be secure. It does this differently depending
on whether or not you've specified C<$webservergroup> in F<localconfig>.

The function that does this is
L<Bugzilla::Install::Filesystem/fix_all_file_permissions>.

=item 12

Populates the C<fielddefs> table, using
L<Bugzilla::Field/populate_field_definitions>.

=item 13

This is the major part of checksetup--updating the table definitions
from one version of Bugzilla to another.

The code for this is in L<Bugzilla::Install::DB/update_table_definitions>.

This includes creating the default Classification (using 
L<Bugzilla::Install/create_default_classification>) and setting up all
the foreign keys for all tables, using L<Bugzilla::DB/bz_setup_foreign_keys>.

=item 14

Creates the system groups--the ones like C<editbugs>, C<admin>, and so on.
This is L<Bugzilla::Install/update_system_groups>.

=item 15

Creates all of the user-adjustable preferences that appear on the
"General Preferences" screen. This is L<Bugzilla::Install/update_settings>.

=item 16

Creates an administrator, if one doesn't already exist, using
L<Bugzilla::Install/create_admin>.

We also can make somebody an admin at this step, if the user specified
the C<--make-admin> switch.

=item 17

Creates the default Product and Component, using
L<Bugzilla::Install/create_default_product>.

=back

=head2 Modifying the Database

Sometimes you'll want to modify the database. In fact, that's mostly
what checksetup does, is upgrade old Bugzilla databases to the modern
format.

If you'd like to know how to make changes to the datbase, see
the information in the Bugzilla Developer's Guide, at:
L<http://www.bugzilla.org/docs/developer.html#sql-schema>

Also see L<Bugzilla::DB/"Schema Modification Methods"> and
L<Bugzilla::DB/"Schema Information Methods">.

=head1 RUNNING CHECKSETUP NON-INTERACTIVELY

To operate checksetup non-interactively, run it with a single argument
specifying a filename that contains the information usually obtained by
prompting the user or by editing localconfig.

The format of that file is as follows:

 $answer{'db_host'}   = 'localhost';
 $answer{'db_driver'} = 'mydbdriver';
 $answer{'db_port'}   = 0;
 $answer{'db_name'}   = 'mydbname';
 $answer{'db_user'}   = 'mydbuser';
 $answer{'db_pass'}   = 'mydbpass';

 $answer{'urlbase'} = 'http://bugzilla.mydomain.com/';

 (Any localconfig variable or parameter can be specified as above.)

 $answer{'ADMIN_EMAIL'} = 'myadmin@mydomain.net';
 $answer{'ADMIN_PASSWORD'} = 'fooey';
 $answer{'ADMIN_REALNAME'} = 'Joel Peshkin';

 $answer{'NO_PAUSE'} = 1

C<NO_PAUSE> means "never stop and prompt the user to hit Enter to continue,
just go ahead and do things, even if they are potentially dangerous." 
Don't set this to 1 unless you know what you are doing.

=head1 SEE ALSO

=over

=item *

L<Bugzilla::Install::Requirements>

=item *

L<Bugzilla::Install::Localconfig>

=item *

L<Bugzilla::Install::Filesystem>

=item *

L<Bugzilla::Install::DB>

=item *

L<Bugzilla::Install>

=item *

L<Bugzilla::Config/update_params>

=item *

L<Bugzilla::DB/CONNECTION>

=back
