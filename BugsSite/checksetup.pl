#!/usr/bin/perl -w
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#                 Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Dave Miller <justdave@syndicomm.com>
#                 Zach Lipton  <zach@zachlipton.com>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Tobias Burnus <burnus@net-b.de>
#                 Shane H. W. Travis <travis@sedsystems.ca>
#                 Gervase Markham <gerv@gerv.net>
#                 Erik Stambaugh <erik@dasbistro.com>
#                 Dave Lawrence <dkl@redhat.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Marc Schumann <wurblzap@gmail.com>
#
#
#
# Hey, what's this?
#
# 'checksetup.pl' is a script that is supposed to run during installation
# time and also after every upgrade.
#
# The goal of this script is to make the installation even easier.
# It does this by doing things for you as well as testing for problems
# in advance.
#
# You can run the script whenever you like. You SHOULD run it after 
# you update Bugzilla, because it may then update your SQL table
# definitions to resync them with the code.
#
# Currently, this module does the following:
#
#     - check for required perl modules
#     - set defaults for local configuration variables
#     - create and populate the data directory after installation
#     - set the proper rights for the *.cgi, *.html, etc. files
#     - verify that the code can access the database server
#     - creates the database 'bugs' if it does not exist
#     - creates the tables inside the database if they don't exist
#     - automatically changes the table definitions if they are from
#       an older version of Bugzilla
#     - populates the groups
#     - put the first user into all groups so that the system can
#       be administrated
#     - changes preexisting SQL tables if you change your local
#       settings, e.g. when you add a new platform
#     - ... and a whole lot more.
#
# There should be no need for Bugzilla Administrators to modify
# this script; all user-configurable stuff has been moved 
# into a local configuration file called 'localconfig'. When that file
# in changed and 'checkconfig.pl' is run, then the user's changes
# will be reflected back into the database.
#
# Developers, however, have to modify this file at various places. To
# make this easier, there are some special tags for which one
# can search.
#
#     To                                               Search for
#
#     add/delete local configuration variables         --LOCAL--
#     check for more required modules                  --MODULES--
#     change the defaults for local configuration vars --LOCAL--
#     update the assigned file permissions             --CHMOD--
#     add more database-related checks                 --DATABASE--
#     change table definitions                         --TABLE--
#     add more groups                                  --GROUPS--
#     add user-adjustable sttings                      --SETTINGS--
#     create initial administrator account             --ADMIN--
#
# Note: sometimes those special comments occur more than once. For
# example, --LOCAL-- is used at least 3 times in this code!  --TABLE--
# is also used more than once, so search for each and every occurrence!
#
# To operate checksetup non-interactively, run it with a single argument
# specifying a filename that contains the information usually obtained by
# prompting the user or by editing localconfig.
#
# The format of that file is as follows:
#
#
# $answer{'db_host'} = q[
# $db_host = 'localhost';
# $db_driver = 'mydbdriver';
# $db_port = 3306;
# $db_name = 'mydbname';
# $db_user = 'mydbuser';
# ];
#
# $answer{'db_pass'} = q[$db_pass = 'mydbpass';];
#
# $answer{'ADMIN_OK'} = 'Y';
# $answer{'ADMIN_EMAIL'} = 'myadmin@mydomain.net';
# $answer{'ADMIN_PASSWORD'} = 'fooey';
# $answer{'ADMIN_REALNAME'} = 'Joel Peshkin';
#
# $answer{'SMTP_SERVER'} = 'mail.mydomain.net';
#
#
# Note: Only information that supersedes defaults from LocalVar()
# function calls needs to be specified in this file.
# 

use strict;

BEGIN {
    if ($^O =~ /MSWin32/i) {
        require 5.008001; # for CGI 2.93 or higher
    }
    require 5.006_001;
    use File::Basename;
    chdir dirname($0);
}

use lib ".";

use vars qw( $db_name %answer );
use Bugzilla::Constants;

my $silent;

my %switch;
$switch{'no_templates'} = grep(/^--no-templates$/, @ARGV) 
    || grep(/^-t$/, @ARGV);

# The use of some Bugzilla modules brings in modules we need to test for
# Check first, via BEGIN
BEGIN {

    # However, don't run under -c (because of tests)
    if (!$^C) {

###########################################################################
# Check for help request. Display help page if --help/-h/-? was passed.
###########################################################################
my $help = grep(/^--help$/, @ARGV) || grep (/^-h$/, @ARGV) || grep (/^-\?$/, @ARGV) || 0;
help_page() if $help;

sub help_page {
    my $programname = $0;
    $programname =~ s#^\./##;
    print "$programname - checks your setup and updates your Bugzilla installation\n";
    print "\nUsage: $programname [SCRIPT [--verbose]] [--check-modules|--help]\n";
    print "                     [--no-templates]\n";
    print "\n";
    print "   --help           Display this help text.\n";
    print "   --check-modules  Only check for correct module dependencies and quit thereafter;\n";
    print "                    does not perform any changes.\n";
    print "   --no-templates (-t)  Don't compile the templates at all. Existing\n"; 
    print "                    compiled templates will remain; missing compiled\n";
    print "                    templates will not be created. (Used primarily by\n";
    print "                    developers to speed up checksetup.)  Use this\n";
    print "                    switch at your own risk.\n";
    print "    SCRIPT          Name of script to drive non-interactive mode.\n";
    print "                    This script should define an \%answer hash whose\n"; 
    print "                    keys are variable names and the values answers to\n";
    print "                    all the questions checksetup.pl asks.\n";
    print "                    (See comments at top of $programname for more info.)\n";
    print "   --verbose        Output results of SCRIPT being processed.\n";
    print "\n";

    exit 1;
}

###########################################################################
# Non-interactive override. Pass a filename on the command line which is
# a Perl script. This script defines a %answer hash whose names are tags
# and whose values are answers to all the questions checksetup.pl asks. 
# Grep this file for references to that hash to see the tags to use for the 
# possible answers. One example is ADMIN_EMAIL.
###########################################################################
if ($ARGV[0] && ($ARGV[0] !~ /^-/)) {
    do $ARGV[0] 
        or ($@ && die("Error $@ processing $ARGV[0]"))
        or die("Error $! processing $ARGV[0]");
    $silent = !grep(/^--no-silent$/, @ARGV) && !grep(/^--verbose$/, @ARGV);
}

###########################################################################
# Check required module
###########################################################################

#
# Here we check for --MODULES--
#

print "\nChecking perl modules ...\n" unless $silent;

# vers_cmp is adapted from Sort::Versions 1.3 1996/07/11 13:37:00 kjahds,
# which is not included with Perl by default, hence the need to copy it here.
# Seems silly to require it when this is the only place we need it...
sub vers_cmp {
    if (@_ < 2) { die "not enough parameters for vers_cmp" }
    if (@_ > 2) { die "too many parameters for vers_cmp" }
    my ($a, $b) = @_;
    my (@A) = ($a =~ /(\.|\d+|[^\.\d]+)/g);
    my (@B) = ($b =~ /(\.|\d+|[^\.\d]+)/g);
    my ($A,$B);
    while (@A and @B) {
        $A = shift @A;
        $B = shift @B;
        if ($A eq "." and $B eq ".") {
            next;
        } elsif ( $A eq "." ) {
            return -1;
        } elsif ( $B eq "." ) {
            return 1;
        } elsif ($A =~ /^\d+$/ and $B =~ /^\d+$/) {
            return $A <=> $B if $A <=> $B;
        } else {
            $A = uc $A;
            $B = uc $B;
            return $A cmp $B if $A cmp $B;
        }
    }
    @A <=> @B;
}

# This was originally clipped from the libnet Makefile.PL, adapted here to
# use the above vers_cmp routine for accurate version checking.
sub have_vers {
    my ($pkg, $wanted) = @_;
    my ($msg, $vnum, $vstr);
    no strict 'refs';
    printf("Checking for %15s %-9s ", $pkg, !$wanted?'(any)':"(v$wanted)") unless $silent;

    # Modules may change $SIG{__DIE__} and $SIG{__WARN__}, so localise them here
    # so that later errors display 'normally'
    local $::SIG{__DIE__};
    local $::SIG{__WARN__};

    eval "require $pkg;";

    # do this twice to avoid a "used only once" error for these vars
    $vnum = ${"${pkg}::VERSION"} || ${"${pkg}::Version"} || 0;
    $vnum = ${"${pkg}::VERSION"} || ${"${pkg}::Version"} || 0;
    $vnum = -1 if $@;

    # CGI's versioning scheme went 2.75, 2.751, 2.752, 2.753, 2.76
    # That breaks the standard version tests, so we need to manually correct
    # the version
    if ($pkg eq 'CGI' && $vnum =~ /(2\.7\d)(\d+)/) {
        $vnum = $1 . "." . $2;
    }

    if ($vnum eq "-1") { # string compare just in case it's non-numeric
        $vstr = "not found";
      }
    elsif (vers_cmp($vnum,"0") > -1) {
        $vstr = "found v$vnum";
    }
    else {
        $vstr = "found unknown version";
    }

    my $vok = (vers_cmp($vnum,$wanted) > -1);
    print ((($vok) ? "ok: " : " "), "$vstr\n") unless $silent;
    return $vok;
}

# Check versions of dependencies.  0 for version = any version acceptable
my $modules = [ 
    { 
        name => 'AppConfig',  
        version => '1.52' 
    }, 
    { 
        name => 'CGI', 
        version => '2.93' 
    }, 
    {
        name => 'Data::Dumper', 
        version => '0' 
    }, 
    {        
        name => 'Date::Format', 
        version => '2.21' 
    }, 
    { 
        name => 'DBI', 
        version => '1.38' 
    }, 
    { 
        name => 'File::Spec', 
        version => '0.84' 
    }, 
    {
        name => 'File::Temp',
        version => '0'
    },
    { 
        name => 'Template', 
        version => '2.08' 
    }, 
    { 
        name => 'Text::Wrap',
        version => '2001.0131'
    }, 
    { 
        name => 'Mail::Mailer', 
        version => '1.65'
    },
    {
        name => 'Storable',
        version => '0'
    },
];

my %ppm_modules = (
    'AppConfig'         => 'AppConfig',
    'Chart::Base'       => 'Chart',
    'CGI'               => 'CGI',
    'Data::Dumper'      => 'Data-Dumper',
    'Date::Format'      => 'TimeDate',
    'DBI'               => 'DBI',
    'DBD::mysql'        => 'DBD-mysql',
    'Template'          => 'Template-Toolkit',
    'PatchReader'       => 'PatchReader',
    'GD'                => 'GD',
    'GD::Graph'         => 'GDGraph',
    'GD::Text::Align'   => 'GDTextUtil',
    'Mail::Mailer'      => 'MailTools',
);

sub install_command {
    my $module = shift;
    if ($^O =~ /MSWin32/i) {
        return "ppm install " . $ppm_modules{$module} if exists $ppm_modules{$module};
        return "ppm install " . $module;
    } else {
        return "$^X -MCPAN -e 'install \"$module\"'";
    }    
}

$::root = ($^O =~ /MSWin32/i ? 'Administrator' : 'root');

my %missing = ();

foreach my $module (@{$modules}) {
    unless (have_vers($module->{name}, $module->{version})) { 
        $missing{$module->{name}} = $module->{version};
    }
}

print "\nThe following Perl modules are optional:\n" unless $silent;
my $gd          = have_vers("GD","1.20");
my $chartbase   = have_vers("Chart::Base","1.0");
my $xmlparser   = have_vers("XML::Parser",0);
my $gdgraph     = have_vers("GD::Graph",0);
my $gdtextalign = have_vers("GD::Text::Align",0);
my $patchreader = have_vers("PatchReader","0.9.4");

print "\n" unless $silent;

if ($^O =~ /MSWin32/i && !$silent) {
    print "All the required modules are available at:\n";
    print "    http://landfill.bugzilla.org/ppm/\n";
    print "You can add the repository with the following command:\n";
    print "    ppm rep add bugzilla http://landfill.bugzilla.org/ppm/\n\n";
}

if ((!$gd || !$chartbase) && !$silent) {
    print "If you you want to see graphical bug charts (plotting historical ";
    print "data over \ntime), you should install libgd and the following Perl ";
    print "modules:\n\n";
    print "GD:          " . install_command("GD") ."\n" if !$gd;
    print "Chart:       " . install_command("Chart::Base") . "\n" if !$chartbase;
    print "\n";
}
if (!$xmlparser && !$silent) {
    print "If you want to use the bug import/export feature to move bugs to\n",
          "or from other bugzilla installations, you will need to install\n ",
          "the XML::Parser module by running (as $::root):\n\n",
    "   " . install_command("XML::Parser") . "\n\n";
}
if ((!$gd || !$gdgraph || !$gdtextalign) && !$silent) {
    print "If you you want to see graphical bug reports (bar, pie and line ";
    print "charts of \ncurrent data), you should install libgd and the ";
    print "following Perl modules:\n\n";
    print "GD:              " . install_command("GD") . "\n" if !$gd;
    print "GD::Graph:       " . install_command("GD::Graph") . "\n" 
        if !$gdgraph;
    print "GD::Text::Align: " . install_command("GD::Text::Align") . "\n"
        if !$gdtextalign;
    print "\n";
}
if (!$patchreader && !$silent) {
    print "If you want to see pretty HTML views of patches, you should ";
    print "install the \nPatchReader module:\n";
    print "PatchReader: " . install_command("PatchReader") . "\n";
}

if (%missing) {
    print "\n\n";
    print "Bugzilla requires some Perl modules which are either missing from\n",
          "your system, or the version on your system is too old.\n",
          "They can be installed by running (as $::root) the following:\n";
    foreach my $module (keys %missing) {
        print "   " . install_command("$module") . "\n";
        if ($missing{$module} > 0) {
            print "   Minimum version required: $missing{$module}\n";
        }
    }
    print "\n";
    exit;
}

}
}

# Break out if checking the modules is all we have been asked to do.
exit if grep(/^--check-modules$/, @ARGV);

# If we're running on Windows, reset the input line terminator so that 
# console input works properly - loading CGI tends to mess it up

if ($^O =~ /MSWin/i) {
    $/ = "\015\012";
}

###########################################################################
# Global definitions
###########################################################################

use Bugzilla::Config qw(:DEFAULT :admin :locations);

# 12/17/00 justdave@syndicomm.com - removed declarations of the localconfig
# variables from this location.  We don't want these declared here.  They'll
# automatically get declared in the process of reading in localconfig, and
# this way we can look in the symbol table to see if they've been declared
# yet or not.

###########################################################################
# Check and update local configuration
###########################################################################

#
# This is quite tricky. But fun!
#
# First we read the file 'localconfig'. Then we check if the variables we
# need are defined. If not, we will append the new settings to
# localconfig, instruct the user to check them, and stop.
#
# Why do it this way?
#
# Assume we will enhance Bugzilla and eventually more local configuration
# stuff arises on the horizon.
#
# But the file 'localconfig' is not in the Bugzilla CVS or tarfile. You
# know, we never want to overwrite your own version of 'localconfig', so
# we can't put it into the CVS/tarfile, can we?
#
# Now, when we need a new variable, we simply add the necessary stuff to
# checksetup. When the user gets the new version of Bugzilla from CVS and
# runs checksetup, it finds out "Oh, there is something new". Then it adds
# some default value to the user's local setup and informs the user to
# check that to see if it is what the user wants.
#
# Cute, ey?
#

print "Checking user setup ...\n" unless $silent;
$@ = undef;
do $localconfig;
if ($@) { # capture errors in localconfig, bug 97290
   print STDERR <<EOT;
An error has occurred while reading your 
'localconfig' file.  The text of the error message is:

$@

Please fix the error in your 'localconfig' file.  
Alternately rename your 'localconfig' file, rerun 
checksetup.pl, and re-enter your answers.

  \$ mv -f localconfig localconfig.old
  \$ ./checksetup.pl


EOT
    die "Syntax error in localconfig";
}

sub LocalVarExists ($)
{
    my ($name) = @_;
    return $main::{$name}; # if localconfig declared it, we're done.
}

my $newstuff = "";
sub LocalVar ($$)
{
    my ($name, $definition) = @_;
    return if LocalVarExists($name); # if localconfig declared it, we're done.
    $newstuff .= " " . $name;
    open FILE, '>>', $localconfig;
    print FILE ($answer{$name} or $definition), "\n\n";
    close FILE;
}


#
# Set up the defaults for the --LOCAL-- variables below:
#

LocalVar('index_html', <<'END');
#
# With the introduction of a configurable index page using the
# template toolkit, Bugzilla's main index page is now index.cgi.
# Most web servers will allow you to use index.cgi as a directory
# index, and many come preconfigured that way, but if yours doesn't
# then you'll need an index.html file that provides redirection
# to index.cgi. Setting $index_html to 1 below will allow
# checksetup.pl to create one for you if it doesn't exist.
# NOTE: checksetup.pl will not replace an existing file, so if you
#       wish to have checksetup.pl create one for you, you must
#       make sure that index.html doesn't already exist
$index_html = 0;
END


if (!LocalVarExists('cvsbin')) {
    my $cvs_executable;
    if ($^O !~ /MSWin32/i) {
        $cvs_executable = `which cvs`;
        if ($cvs_executable =~ /no cvs/ || $cvs_executable eq '') {
            # If which didn't find it, just set to blank
            $cvs_executable = "";
        } else {
            chomp $cvs_executable;
        }
    } else {
        $cvs_executable = "";
    }

    LocalVar('cvsbin', <<"END");
#
# For some optional functions of Bugzilla (such as the pretty-print patch
# viewer), we need the cvs binary to access files and revisions.
# Because it's possible that this program is not in your path, you can specify
# its location here.  Please specify the full path to the executable.
\$cvsbin = "$cvs_executable";
END
}


if (!LocalVarExists('interdiffbin')) {
    my $interdiff_executable;
    if ($^O !~ /MSWin32/i) {
        $interdiff_executable = `which interdiff`;
        if ($interdiff_executable =~ /no interdiff/ ||
            $interdiff_executable eq '') {
            if (!$silent) {
                print "\nOPTIONAL NOTE: If you want to be able to ";
                print "use the\n 'difference between two patches' feature";
                print "of Bugzilla (requires\n the PatchReader Perl module ";
                print "as well), you should install\n patchutils from ";
                print "http://cyberelk.net/tim/patchutils/\n\n";
            }

            # If which didn't find it, set to blank
            $interdiff_executable = "";
        } else {
            chomp $interdiff_executable;
        }
    } else {
        $interdiff_executable = "";
    }

    LocalVar('interdiffbin', <<"END");

#
# For some optional functions of Bugzilla (such as the pretty-print patch
# viewer), we need the interdiff binary to make diffs between two patches.
# Because it's possible that this program is not in your path, you can specify
# its location here.  Please specify the full path to the executable.
\$interdiffbin = "$interdiff_executable";
END
}


if (!LocalVarExists('diffpath')) {
    my $diff_binaries;
    if ($^O !~ /MSWin32/i) {
        $diff_binaries = `which diff`;
        if ($diff_binaries =~ /no diff/ || $diff_binaries eq '') {
            # If which didn't find it, set to blank
            $diff_binaries = "";
        } else {
            $diff_binaries =~ s:/diff\n$::;
        }
    } else {
        $diff_binaries = "";
    }

    LocalVar('diffpath', <<"END");

#
# The interdiff feature needs diff, so we have to have that path.
# Please specify the directory name only; do not use trailing slash.
\$diffpath = "$diff_binaries";
END
}


LocalVar('create_htaccess', <<'END');
#
# If you are using Apache as your web server, Bugzilla can create .htaccess
# files for you that will instruct Apache not to serve files that shouldn't
# be accessed from the web (like your local configuration data and non-cgi
# executable files).  For this to work, the directory your Bugzilla
# installation is in must be within the jurisdiction of a <Directory> block
# in the httpd.conf file that has 'AllowOverride Limit' in it.  If it has
# 'AllowOverride All' or other options with Limit, that's fine.
# (Older Apache installations may use an access.conf file to store these
# <Directory> blocks.)
# If this is set to 1, Bugzilla will create these files if they don't exist.
# If this is set to 0, Bugzilla will not create these files.
$create_htaccess = 1;
END

my $webservergroup_default;
if ($^O !~ /MSWin32/i) {
    $webservergroup_default = 'apache';
} else {
    $webservergroup_default = '';
}

LocalVar('webservergroup', <<"END");
#
# This is the group your web server runs as.
# If you have a windows box, ignore this setting.
# If you do not have access to the group your web server runs under,
# set this to "". If you do set this to "", then your Bugzilla installation
# will be _VERY_ insecure, because some files will be world readable/writable,
# and so anyone who can get local access to your machine can do whatever they
# want. You should only have this set to "" if this is a testing installation
# and you cannot set this up any other way. YOU HAVE BEEN WARNED!
# If you set this to anything other than "", you will need to run checksetup.pl
# as $::root, or as a user who is a member of the specified group.
\$webservergroup = "$webservergroup_default";
END



LocalVar('db_driver', '
#
# What SQL database to use. Default is mysql. List of supported databases
# can be obtained by listing Bugzilla/DB directory - every module corresponds
# to one supported database and the name corresponds to a driver name.
#
$db_driver = "mysql";
');
LocalVar('db_host', q[
#
# How to access the SQL database:
#
$db_host = 'localhost';         # where is the database?
$db_name = 'bugs';              # name of the SQL database
$db_user = 'bugs';              # user to attach to the SQL database

# Sometimes the database server is running on a non-standard
# port. If that's the case for your database server, set this
# to the port number that your database server is running on.
# Setting this to 0 means "use the default port for my database
# server."
$db_port = 0;
]);
LocalVar('db_pass', q[
#
# Enter your database password here. It's normally advisable to specify
# a password for your bugzilla database user.
# If you use apostrophe (') or a backslash (\) in your password, you'll
# need to escape it by preceding it with a '\' character. (\') or (\\)
# (Far simpler just not to use those characters.)
#
$db_pass = '';
]);

LocalVar('db_sock', q[
# MySQL Only: Enter a path to the unix socket for mysql. If this is 
# blank, then mysql\'s compiled-in default will be used. You probably 
# want that.
$db_sock = '';
]);

LocalVar('db_check', q[
#
# Should checksetup.pl try to verify that your database setup is correct?
# (with some combinations of database servers/Perl modules/moonphase this 
# doesn't work)
#
$db_check = 1;
]);

my @deprecatedvars;
push(@deprecatedvars, '@severities') if (LocalVarExists('severities'));
push(@deprecatedvars, '@priorities') if (LocalVarExists('priorities'));
push(@deprecatedvars, '@opsys') if (LocalVarExists('opsys'));
push(@deprecatedvars, '@platforms') if (LocalVarExists('platforms'));

if (@deprecatedvars) {
    print "\nThe following settings in your localconfig file",
          " are no longer used:\n  " . join(", ", @deprecatedvars) .
          "\nThis data is now controlled through the Bugzilla",
          " administrative interface.\nWe recommend you remove these",
          " settings from localconfig after checksetup\nruns successfully.\n";
}
if (LocalVarExists('mysqlpath')) {
    print "\nThe \$mysqlpath setting in your localconfig file ",
          "is no longer required.\nWe recommend you remove it.\n";
}

if ($newstuff ne "") {
    print "\nThis version of Bugzilla contains some variables that you may \n",
          "want to change and adapt to your local settings. Please edit the\n",
          "file '$localconfig' and rerun checksetup.pl\n\n",
          "The following variables are new to localconfig since you last ran\n",
          "checksetup.pl:  $newstuff\n\n";
    exit;
}

# 2000-Dec-18 - justdave@syndicomm.com - see Bug 52921
# This is a hack to read in the values defined in localconfig without getting
# them defined at compile time if they're missing from localconfig.
# Ideas swiped from pp. 281-282, O'Reilly's "Programming Perl 2nd Edition"
# Note that we won't need to do this in globals.pl because globals.pl couldn't
# care less whether they were defined ahead of time or not. 
my $my_db_check = ${*{$main::{'db_check'}}{SCALAR}};
my $my_db_driver = ${*{$main::{'db_driver'}}{SCALAR}};
my $my_db_name = ${*{$main::{'db_name'}}{SCALAR}};
my $my_index_html = ${*{$main::{'index_html'}}{SCALAR}};
my $my_create_htaccess = ${*{$main::{'create_htaccess'}}{SCALAR}};
my $my_webservergroup = ${*{$main::{'webservergroup'}}{SCALAR}};

if ($my_webservergroup && !$silent) {
    if ($^O !~ /MSWin32/i) {
        # if on unix, see if we need to print a warning about a webservergroup
        # that we can't chgrp to
        my $webservergid = (getgrnam($my_webservergroup))[2]
                           or die("no such group: $my_webservergroup");
        if ($< != 0 && !grep($_ eq $webservergid, split(" ", $)))) {
            print <<EOF;

Warning: you have entered a value for the "webservergroup" parameter in 
localconfig, but you are not either a) running this script as $::root; or b) a 
member of this group. This can cause permissions problems and decreased 
security.  If you experience problems running Bugzilla scripts, log in as 
$::root and re-run this script, become a member of the group, or remove the 
value of the "webservergroup" parameter. Note that any warnings about 
"uninitialized values" that you may see below are caused by this.

EOF
        }
    }

    else {
        # if on Win32, print a reminder that setting this value adds no security
        print <<EOF;
      
Warning: You have set webservergroup in your localconfig.
Please understand that this does not bring you any security when
running under Windows.
Verify that the file permissions in your Bugzilla directory are
suitable for your system.
Avoid unnecessary write access.

EOF
    }

} else {
    # There's no webservergroup, this is very very very very bad.
    # However, if we're being run on windows, then this option doesn't
    # really make sense. Doesn't make it any more secure either, though,
    # but don't print the message, since they can't do anything about it.
    if (($^O !~ /MSWin32/i) && !$silent) {
        print <<EOF;

********************************************************************************
WARNING! You have not entered a value for the "webservergroup" parameter
in localconfig. This means that certain files and directories which need
to be editable by both you and the webserver must be world writable, and
other files (including the localconfig file which stores your database
password) must be world readable. This means that _anyone_ who can obtain
local access to this machine can do whatever they want to your Bugzilla
installation, and is probably also able to run arbitrary Perl code as the
user that the webserver runs as.

You really, really, really need to change this setting.
********************************************************************************

EOF
    }
}

###########################################################################
# Check data directory
###########################################################################

#
# Create initial --DATA-- directory and make the initial empty files there:
#

# The |require "globals.pl"| above ends up creating a template object with
# a COMPILE_DIR of "$datadir". This means that TT creates the directory for us,
# so this code wouldn't run if we just checked for the existence of the
# directory. Instead, check for the existence of '$datadir/nomail', which is
# created in this block
unless (-d $datadir && -e "$datadir/nomail") {
    print "Creating data directory ($datadir) ...\n";
    # permissions for non-webservergroup are fixed later on
    mkdir $datadir, 0770;
    mkdir "$datadir/mimedump-tmp", 01777;
    open FILE, '>>', "$datadir/nomail"; close FILE;
    open FILE, '>>', "$datadir/mail"; close FILE;
}


unless (-d $attachdir) {
    print "Creating local attachments directory ...\n";
    # permissions for non-webservergroup are fixed later on
    mkdir $attachdir, 0770;
}


# 2000-12-14 New graphing system requires a directory to put the graphs in
# This code copied from what happens for the data dir above.
# If the graphs dir is not present, we assume that they have been using
# a Bugzilla with the old data format, and upgrade their data files.

# NB - the graphs dir isn't movable yet, unlike the datadir
unless (-d 'graphs') {
    print "Creating graphs directory...\n";
    # permissions for non-webservergroup are fixed later on
    mkdir 'graphs', 0770;
    # Upgrade data format
    foreach my $in_file (glob("$datadir/mining/*"))
    {
        # Don't try and upgrade image or db files!
        if (($in_file =~ /\.gif$/i) || 
            ($in_file =~ /\.png$/i) ||
            ($in_file =~ /\.db$/i) ||
            ($in_file =~ /\.orig$/i)) {
            next;
        }

        rename("$in_file", "$in_file.orig") or next;        
        open(IN, "$in_file.orig") or next;
        open(OUT, '>', $in_file) or next;
        
        # Fields in the header
        my @declared_fields = ();

        # Fields we changed to half way through by mistake
        # This list comes from an old version of collectstats.pl
        # This part is only for people who ran later versions of 2.11 (devel)
        my @intermediate_fields = qw(DATE UNCONFIRMED NEW ASSIGNED REOPENED 
                                     RESOLVED VERIFIED CLOSED);

        # Fields we actually want (matches the current collectstats.pl)
        my @out_fields = qw(DATE NEW ASSIGNED REOPENED UNCONFIRMED RESOLVED
                            VERIFIED CLOSED FIXED INVALID WONTFIX LATER REMIND
                            DUPLICATE WORKSFORME MOVED);

        while (<IN>) {
            if (/^# fields?: (.*)\s$/) {
                @declared_fields = map uc, (split /\||\r/, $1);
                print OUT "# fields: ", join('|', @out_fields), "\n";
            }
            elsif (/^(\d+\|.*)/) {
                my @data = split /\||\r/, $1;
                my %data = ();
                if (@data == @declared_fields) {
                    # old format
                    for my $i (0 .. $#declared_fields) {
                        $data{$declared_fields[$i]} = $data[$i];
                    }
                }
                elsif (@data == @intermediate_fields) {
                    # Must have changed over at this point 
                    for my $i (0 .. $#intermediate_fields) {
                        $data{$intermediate_fields[$i]} = $data[$i];
                    }
                }
                elsif (@data == @out_fields) {
                    # This line's fine - it has the right number of entries 
                    for my $i (0 .. $#out_fields) {
                        $data{$out_fields[$i]} = $data[$i];
                    }
                }
                else {
                    print "Oh dear, input line $. of $in_file had " . 
                      scalar(@data) . " fields\n";
                    print "This was unexpected. You may want to check your data files.\n";
                }

                print OUT join('|', map { 
                              defined ($data{$_}) ? ($data{$_}) : "" 
                                                          } @out_fields), "\n";
            }
            else {
                print OUT;
            }
        }

        close(IN);
        close(OUT);
    }
}

unless (-d "$datadir/mining") {
    mkdir "$datadir/mining", 0700;
}

unless (-d "$webdotdir") {
    # perms/ownership are fixed up later
    mkdir "$webdotdir", 0700;
}

if (!-d "skins/custom") {
    # perms/ownership are fixed up later
    mkdir "skins/custom", 0700;
}

if (!-e "skins/.cvsignore") {
    open CVSIGNORE, '>>', "skins/.cvsignore";
    print CVSIGNORE ".cvsignore\n";
    print CVSIGNORE "custom\n";
    close CVSIGNORE;
}

# Create custom stylesheets for each standard stylesheet.
foreach my $standard (<skins/standard/*.css>) {
    my $custom = $standard;
    $custom =~ s|^skins/standard|skins/custom|;
    if (!-e $custom) {
        open STYLESHEET, '>', $custom;
        print STYLESHEET <<"END";
/* 
 * Custom rules for $standard.
 * The rules you put here override rules in that stylesheet.
 */
END
        close STYLESHEET;
    }
}

if ($my_create_htaccess) {
  my $fileperm = 0644;
  my $dirperm = 01777;
  if ($my_webservergroup) {
    $fileperm = 0640;
    $dirperm = 0770;
  }
  if (!-e ".htaccess") {
    print "Creating .htaccess...\n";
    open HTACCESS, '>', '.htaccess';
    print HTACCESS <<'END';
# don't allow people to retrieve non-cgi executable files or our private data
<FilesMatch ^(.*\.pm|.*\.pl|.*localconfig.*)$>
  deny from all
</FilesMatch>
<FilesMatch ^(localconfig.js|localconfig.rdf)$>
  allow from all
</FilesMatch>
END
    close HTACCESS;
    chmod $fileperm, ".htaccess";
  } else {
    # 2002-12-21 Bug 186383
    open HTACCESS, ".htaccess";
    my $oldaccess = "";
    while (<HTACCESS>) {
      $oldaccess .= $_;
    }
    close HTACCESS;
    my $repaired = 0;
    if ($oldaccess =~ s/\|localconfig\|/\|.*localconfig.*\|/) {
        $repaired = 1;
    }
    if ($oldaccess !~ /\(\.\*\\\.pm\|/) {
        $oldaccess =~ s/\(/(.*\\.pm\|/;
        $repaired = 1;
    }
    if ($repaired) {
      print "Repairing .htaccess...\n";
      open HTACCESS, '>', '.htaccess';
      print HTACCESS $oldaccess;
      print HTACCESS <<'END';
<FilesMatch ^(localconfig.js|localconfig.rdf)$>
  allow from all
</FilesMatch>
END
      close HTACCESS;
    }

  }
  if (!-e "$attachdir/.htaccess") {
    print "Creating $attachdir/.htaccess...\n";
    open HTACCESS, ">$attachdir/.htaccess";
    print HTACCESS <<'END';
# nothing in this directory is retrievable unless overridden by an .htaccess
# in a subdirectory;
deny from all
END
    close HTACCESS;
    chmod $fileperm, "$attachdir/.htaccess";
  }
  if (!-e "Bugzilla/.htaccess") {
    print "Creating Bugzilla/.htaccess...\n";
    open HTACCESS, '>', 'Bugzilla/.htaccess';
    print HTACCESS <<'END';
# nothing in this directory is retrievable unless overridden by an .htaccess
# in a subdirectory
deny from all
END
    close HTACCESS;
    chmod $fileperm, "Bugzilla/.htaccess";
  }
  # Even though $datadir may not (and should not) be in the webtree,
  # we can't know for sure, so create the .htaccess anyeay. Its harmless
  # if its not accessible...
  if (!-e "$datadir/.htaccess") {
    print "Creating $datadir/.htaccess...\n";
    open HTACCESS, '>', "$datadir/.htaccess";
    print HTACCESS <<'END';
# nothing in this directory is retrievable unless overriden by an .htaccess
# in a subdirectory; the only exception is duplicates.rdf, which is used by
# duplicates.xul and must be loadable over the web
deny from all
<Files duplicates.rdf>
  allow from all
</Files>
END
    close HTACCESS;
    chmod $fileperm, "$datadir/.htaccess";
  }
  # Ditto for the template dir
  if (!-e "$templatedir/.htaccess") {
    print "Creating $templatedir/.htaccess...\n";
    open HTACCESS, '>', "$templatedir/.htaccess";
    print HTACCESS <<'END';
# nothing in this directory is retrievable unless overriden by an .htaccess
# in a subdirectory
deny from all
END
    close HTACCESS;
    chmod $fileperm, "$templatedir/.htaccess";
  }
  if (!-e "$webdotdir/.htaccess") {
    print "Creating $webdotdir/.htaccess...\n";
    open HTACCESS, '>', "$webdotdir/.htaccess";
    print HTACCESS <<'END';
# Restrict access to .dot files to the public webdot server at research.att.com 
# if research.att.com ever changes their IP, or if you use a different
# webdot server, you'll need to edit this
<FilesMatch \.dot$>
  Allow from 192.20.225.10
  Deny from all
</FilesMatch>

# Allow access to .png files created by a local copy of 'dot'
<FilesMatch \.png$>
  Allow from all
</FilesMatch>

# And no directory listings, either.
Deny from all
END
    close HTACCESS;
    chmod $fileperm, "$webdotdir/.htaccess";
  }

}

if ($my_index_html) {
    if (!-e "index.html") {
        print "Creating index.html...\n";
        open HTML, '>', 'index.html';
        print HTML <<'END';
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<meta http-equiv="Refresh" content="0; URL=index.cgi">
</head>
<body>
<h1>I think you are looking for <a href="index.cgi">index.cgi</a></h1>
</body>
</html>
END
        close HTML;
    }
    else {
        open HTML, "index.html";
        if (! grep /index\.cgi/, <HTML>) {
            print "\n\n";
            print "*** It appears that you still have an old index.html hanging\n";
            print "    around.  The contents of this file should be moved into a\n";
            print "    template and placed in the 'en/custom' directory within";
            print "    your template directory.\n\n";
        }
        close HTML;
    }
}

# Just to be sure ...
unlink "$datadir/versioncache";

# Remove parameters from the params file that no longer exist in Bugzilla,
# and set the defaults for new ones

my @oldparams = UpdateParams();

if (@oldparams) {
    open(PARAMFILE, '>>', 'old-params.txt') 
      || die "$0: Can't open old-params.txt for writing: $!\n";

    print "The following parameters are no longer used in Bugzilla, " .
          "and so have been\nmoved from your parameters file " .
          "into old-params.txt:\n";

    foreach my $p (@oldparams) {
        my ($item, $value) = @{$p};

        print PARAMFILE "\n\n$item:\n$value\n";

        print $item;
        print ", " unless $item eq $oldparams[$#oldparams]->[0];
    }
    print "\n";
    close PARAMFILE;
}

# Set mail_delivery_method to SMTP and prompt for SMTP server
# if running on Windows and no third party sendmail wrapper
# is available
if ($^O =~ /MSWin32/i
    && Param('mail_delivery_method') eq 'sendmail'
    && !-e SENDMAIL_EXE)
{
    print "\nBugzilla requires an SMTP server to function on Windows.\n" .
        "Please enter your SMTP server's hostname: ";
    my $smtp = $answer{'SMTP_SERVER'} 
        || ($silent && die("cant preload SMTP_SERVER")) 
        || <STDIN>;
    chomp $smtp;
    if (!$smtp) {
        print "\nWarning: No SMTP Server provided, defaulting to localhost\n";
        $smtp = 'localhost';
    }
    SetParam('mail_delivery_method', 'smtp');
    SetParam('smtpserver', $smtp);
}

# WriteParams will only write out still-valid entries
WriteParams();

unless ($switch{'no_templates'}) {
    if (-e "$datadir/template") {
        print "Removing existing compiled templates ...\n" unless $silent;

       File::Path::rmtree("$datadir/template");

       #Check that the directory was really removed
       if(-e "$datadir/template") {
           print "\n\n";
           print "The directory '$datadir/template' could not be removed.\n";
           print "Please remove it manually and rerun checksetup.pl.\n\n";
           exit;
       }
    }

    # Precompile stuff. This speeds up initial access (so the template isn't
    # compiled multiple times simultaneously by different servers), and helps
    # to get the permissions right.
    sub compile {
        my $name = $File::Find::name;

        return if (-d $name);
        return if ($name =~ /\/CVS\//);
        return if ($name !~ /\.tmpl$/);
        $name =~ s/\Q$::templatepath\E\///; # trim the bit we don't pass to TT

        # Compile the template but throw away the result. This has the side-
        # effect of writing the compiled version to disk.
        $::template->context()->template($name);
    }
    
    eval("use Template");

    {
        print "Precompiling templates ...\n" unless $silent;

        use File::Find;
        require Bugzilla::Template;
        
        # Don't hang on templates which use the CGI library
        eval("use CGI qw(-no_debug)");
        
        use File::Spec; 
        opendir(DIR, $templatedir) || die "Can't open '$templatedir': $!";
        my @files = grep { /^[a-z-]+$/i } readdir(DIR);
        closedir DIR;

        foreach my $dir (@files) {
            next if($dir =~ /^CVS$/i);
            -d "$templatedir/$dir/custom" || -d "$templatedir/$dir/default"
                || next;
            local $ENV{'HTTP_ACCEPT_LANGUAGE'} = $dir;
            SetParam("languages", "$dir,en");
            $::template = Bugzilla::Template->create(clean_cache => 1);
            my @templatepaths;
            foreach my $subdir (qw(custom extension default)) {
                $::templatepath = File::Spec->catdir($templatedir, $dir, $subdir);
                next unless -d $::templatepath;
                # Traverse the template hierarchy. 
                find({ wanted => \&compile, no_chdir => 1 }, $::templatepath);
            }
        }
    }
}

###########################################################################
# Set proper rights
###########################################################################

#
# Here we use --CHMOD-- and friends to set the file permissions
#
# The rationale is that the web server generally runs as apache, so the cgi
# scripts should not be writable for apache, otherwise someone may be possible
# to change the cgi's when exploiting some security flaw somewhere (not
# necessarily in Bugzilla!)
#
# Also, some *.pl files are executable, some are not.
#
# +++ Can anybody tell me what a Windows Perl would do with this code?
#
# Changes 03/14/00 by SML
#
# This abstracts out what files are executable and what ones are not.  It makes
# for slightly neater code and lets us do things like determine exactly which
# files are executable and which ones are not.
#
# Not all directories have permissions changed on them.  i.e., changing ./CVS
# to be 0640 is bad.
#
# Fixed bug in chmod invocation.  chmod (at least on my linux box running perl
# 5.005 needs a valid first argument, not 0.
#
# (end changes, 03/14/00 by SML)
#
# Changes 15/06/01 kiko@async.com.br
# 
# Fix file permissions for non-webservergroup installations (see
# http://bugzilla.mozilla.org/show_bug.cgi?id=71555). I'm setting things
# by default to world readable/executable for all files, and
# world-writable (with sticky on) to data and graphs.
#

# These are the files which need to be marked executable
my @executable_files = ('whineatnews.pl', 'collectstats.pl',
   'checksetup.pl', 'importxml.pl', 'runtests.pl', 'testserver.pl',
   'whine.pl');

# tell me if a file is executable.  All CGI files and those in @executable_files
# are executable
sub isExecutableFile {
    my ($file) = @_;
    if ($file =~ /\.cgi/) {
        return 1;
    }

    my $exec_file;
    foreach $exec_file (@executable_files) {
        if ($file eq $exec_file) {
            return 1;
        }
    }
    return undef;
}

# fix file (or files - wildcards ok) permissions 
sub fixPerms {
    my ($file_pattern, $owner, $group, $umask, $do_dirs) = @_;
    my @files = glob($file_pattern);
    my $execperm = 0777 & ~ $umask;
    my $normperm = 0666 & ~ $umask;
    foreach my $file (@files) {
        next if (!-e $file);
        # do not change permissions on directories here unless $do_dirs is set
        if (!(-d $file)) {
            chown $owner, $group, $file;
            # check if the file is executable.
            if (isExecutableFile($file)) {
                #printf ("Changing $file to %o\n", $execperm);
                chmod $execperm, $file;
            } else {
                #printf ("Changing $file to %o\n", $normperm);
                chmod $normperm, $file;
            }
        }
        elsif ($do_dirs) {
            chown $owner, $group, $file;
            if ($file =~ /CVS$/) {
                chmod 0700, $file;
            }
            else {
                #printf ("Changing $file to %o\n", $execperm);
                chmod $execperm, $file;
                fixPerms("$file/.htaccess", $owner, $group, $umask, $do_dirs);
                # do the contents of the directory
                fixPerms("$file/*", $owner, $group, $umask, $do_dirs); 
            }
        }
    }
}

if ($^O !~ /MSWin32/i) {
    if ($my_webservergroup) {
        # Funny! getgrname returns the GID if fed with NAME ...
        my $webservergid = getgrnam($my_webservergroup) 
        or die("no such group: $my_webservergroup");
        # chown needs to be called with a valid uid, not 0.  $< returns the
        # caller's uid.  Maybe there should be a $bugzillauid, and call 
        # with that userid.
        fixPerms('.htaccess', $<, $webservergid, 027); # glob('*') doesn't catch dotfiles
        fixPerms("$datadir/.htaccess", $<, $webservergid, 027);
        fixPerms("$datadir/duplicates", $<, $webservergid, 027, 1);
        fixPerms("$datadir/mining", $<, $webservergid, 027, 1);
        fixPerms("$datadir/template", $<, $webservergid, 007, 1); # webserver will write to these
        fixPerms($attachdir, $<, $webservergid, 007, 1); # webserver will write to these
        fixPerms($webdotdir, $<, $webservergid, 007, 1);
        fixPerms("$webdotdir/.htaccess", $<, $webservergid, 027);
        fixPerms("$datadir/params", $<, $webservergid, 017);
        fixPerms('*', $<, $webservergid, 027);
        fixPerms('Bugzilla', $<, $webservergid, 027, 1);
        fixPerms($templatedir, $<, $webservergid, 027, 1);
        fixPerms('images', $<, $webservergid, 027, 1);
        fixPerms('css', $<, $webservergid, 027, 1);
        fixPerms('skins', $<, $webservergid, 027, 1);
        fixPerms('js', $<, $webservergid, 027, 1);
        chmod 0644, 'globals.pl';
        
        # Don't use fixPerms here, because it won't change perms 
        # on the directory unless it's using recursion
        chown $<, $webservergid, $datadir;
        chmod 0771, $datadir;
        chown $<, $webservergid, 'graphs';
        chmod 0770, 'graphs';
    } else {
        # get current gid from $( list
        my $gid = (split " ", $()[0];
        fixPerms('.htaccess', $<, $gid, 022); # glob('*') doesn't catch dotfiles
        fixPerms("$datadir/.htaccess", $<, $gid, 022);
        fixPerms("$datadir/duplicates", $<, $gid, 022, 1);
        fixPerms("$datadir/mining", $<, $gid, 022, 1);
        fixPerms("$datadir/template", $<, $gid, 000, 1); # webserver will write to these
        fixPerms($webdotdir, $<, $gid, 000, 1);
        chmod 01777, $webdotdir;
        fixPerms("$webdotdir/.htaccess", $<, $gid, 022);
        fixPerms("$datadir/params", $<, $gid, 011);
        fixPerms('*', $<, $gid, 022);
        fixPerms('Bugzilla', $<, $gid, 022, 1);
        fixPerms($templatedir, $<, $gid, 022, 1);
        fixPerms('images', $<, $gid, 022, 1);
        fixPerms('css', $<, $gid, 022, 1);
        fixPerms('skins', $<, $gid, 022, 1);
        fixPerms('js', $<, $gid, 022, 1);
        
        # Don't use fixPerms here, because it won't change perms
        # on the directory unless it's using recursion
        chown $<, $gid, $datadir;
        chmod 0777, $datadir;
        chown $<, $gid, 'graphs';
        chmod 01777, 'graphs';
    }
}

###########################################################################
# Global Utility Library
###########################################################################

# This is done here, because some modules require params to be set up, which
# won't have happened earlier.

# It's never safe to "use" a Bugzilla module in checksetup. If a module
# prerequisite is missing, and you "use" a module that requires it,
# then instead of our nice normal checksetup message the user would
# get a cryptic perl error about the missing module.

# This is done so we can add new settings as developers need them.
require Bugzilla::User::Setting;
import Bugzilla::User::Setting qw(add_setting);

require Bugzilla::Util;
import Bugzilla::Util qw(bz_crypt trim html_quote);

require Bugzilla::User;
import Bugzilla::User qw(insert_new_user);

# globals.pl clears the PATH, but File::Find uses Cwd::cwd() instead of
# Cwd::getcwd(), which we need to do because `pwd` isn't in the path - see
# http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2001-09/msg00115.html
# As a workaround, since we only use File::Find in checksetup, which doesn't
# run in taint mode anyway, preserve the path...
my $origPath = $::ENV{'PATH'};

# Use the Bugzilla utility library for various functions.  We do this
# here rather than at the top of the file so globals.pl doesn't define
# localconfig variables for us before we get a chance to check for
# their existence and create them if they don't exist.  Also, globals.pl
# removes $ENV{'path'}, which we need in order to run `which mysql` above.
require "globals.pl";

# ...and restore it. This doesn't change tainting, so this will still cause
# errors if this script ever does run with -T.
$::ENV{'PATH'} = $origPath;

###########################################################################
# Check Database setup
###########################################################################

#
# Check if we have access to the --DATABASE--
#

# No need to "use" this here.  It should already be loaded from the
# version-checking routines above, and this file won't even compile if
# DBI isn't installed so the user gets nasty errors instead of our
# pretty one saying they need to install it. -- justdave@syndicomm.com
#use DBI;

if ($my_db_check) {
    # Do we have the database itself?

    # Unfortunately, $my_db_driver doesn't map perfectly between DBD
    # and Bugzilla::DB. We need to fix the case a bit.
    (my $dbd_name = trim($my_db_driver)) =~ s/(\w+)/\u\L$1/g;
    # And MySQL is special, because it's all lowercase in DBD.
    $dbd_name = 'mysql' if $dbd_name eq 'Mysql';

    my $dbd = "DBD::$dbd_name";
    unless (eval("require $dbd")) {
        print "Bugzilla requires that perl's $dbd be installed.\n"
              . "To install this module, you can do:\n "
              . "   " . install_command($dbd) . "\n";
        exit;
    }

    my $dbh = Bugzilla::DB::connect_main("no database connection");
    my $sql_want = $dbh->REQUIRED_VERSION;
    my $sql_server = $dbh->PROGRAM_NAME;
    my $dbd_ver = $dbh->DBD_VERSION;
    unless (have_vers($dbd, $dbd_ver)) {
        die "Bugzilla requires at least version $dbd_ver of $dbd.";
    }

    printf("Checking for %15s %-9s ", $sql_server, "(v$sql_want)") unless $silent;
    my $sql_vers = $dbh->bz_server_version;

    # Check what version of the database server is installed and let
    # the user know if the version is too old to be used with Bugzilla.
    if ( vers_cmp($sql_vers,$sql_want) > -1 ) {
        print "ok: found v$sql_vers\n" unless $silent;
    } else {
        die "\nYour $sql_server v$sql_vers is too old.\n" . 
            "   Bugzilla requires version $sql_want or later of $sql_server.\n" . 
            "   Please download and install a newer version.\n";
    }
    # This message is specific to MySQL.
    if ($dbh->isa('Bugzilla::DB::Mysql') && ($sql_vers =~ /^4\.0\.(\d+)/) && ($1 < 2)) {
        die "\nYour MySQL server is incompatible with Bugzilla.\n" .
            "   Bugzilla does not support versions 4.x.x below 4.0.2.\n" .
            "   Please visit http://www.mysql.com/ and download a newer version.\n";
    }

    # See if we can connect to the database.
    my $conn_success = eval { 
        my $check_dbh = Bugzilla::DB::connect_main(); 
        $check_dbh->disconnect;
    };
    if (!$conn_success) {
       print "Creating database $my_db_name ...\n";
       # Try to create the DB, and if we fail print an error.
       if (!eval { $dbh->do("CREATE DATABASE $my_db_name") }) {
            my $error = $dbh->errstr;
            die <<"EOF"

The '$my_db_name' database could not be created.  The error returned was:

$error

This might have several reasons:

* $sql_server is not running.
* $sql_server is running, but there is a problem either in the
  server configuration or the database access rights. Read the Bugzilla
  Guide in the doc directory. The section about database configuration
  should help.
* There is a subtle problem with Perl, DBI, or $sql_server. Make
  sure all settings in '$localconfig' are correct. If all else fails, set
  '\$db_check' to zero.\n
EOF
        }
    }
    $dbh->disconnect if $dbh;
}

# now get a handle to the database:
my $dbh = Bugzilla::DB::connect_main();

END { $dbh->disconnect if $dbh }

###########################################################################
# Check for LDAP
###########################################################################

for my $verifymethod (split /,\s*/, Param('user_verify_class')) {
    if ($verifymethod eq 'LDAP') {
        my $netLDAP = have_vers("Net::LDAP", 0);
        if (!$netLDAP && !$silent) {
            print "If you wish to use LDAP authentication, then you must install Net::LDAP\n\n";
        }
    }
}

###########################################################################
# Check GraphViz setup
###########################################################################

#
# If we are using a local 'dot' binary, verify the specified binary exists
# and that the generated images are accessible.
#

if( Param('webdotbase') && Param('webdotbase') !~ /^https?:/ ) {
    printf("Checking for %15s %-9s ", "GraphViz", "(any)") unless $silent;
    if(-x Param('webdotbase')) {
        print "ok: found\n" unless $silent;
    } else {
        print "not a valid executable: " . Param('webdotbase') . "\n";
    }

    # Check .htaccess allows access to generated images
    if(-e "$webdotdir/.htaccess") {
        open HTACCESS, "$webdotdir/.htaccess";
        if(! grep(/png/,<HTACCESS>)) {
            print "Dependency graph images are not accessible.\n";
            print "delete $webdotdir/.htaccess and re-run checksetup.pl to fix.\n";
        }
        close HTACCESS;
    }
}

print "\n" unless $silent;

###########################################################################
# Create tables
###########################################################################

# Note: --TABLE-- definitions are now in Bugzilla::DB::Schema.
$dbh->bz_setup_database();

###########################################################################
# Populate groups table
###########################################################################

sub GroupDoesExist ($)
{
    my ($name) = @_;
    my $sth = $dbh->prepare("SELECT name FROM groups WHERE name='$name'");
    $sth->execute;
    if ($sth->rows) {
        return 1;
    }
    return 0;
}


#
# This subroutine ensures that a group exists. If not, it will be created 
# automatically, and given the next available groupid
#

sub AddGroup {
    my ($name, $desc, $userregexp) = @_;
    $userregexp ||= "";

    return if GroupDoesExist($name);
    
    print "Adding group $name ...\n";
    my $sth = $dbh->prepare('INSERT INTO groups
                          (name, description, userregexp, isbuggroup,
                           last_changed)
                          VALUES (?, ?, ?, ?, NOW())');
    $sth->execute($name, $desc, $userregexp, 0);

    my $last = $dbh->bz_last_key('groups', 'id');
    return $last;
}


###########################################################################
# Populate the list of fields.
###########################################################################

my $headernum = 1;

sub AddFDef ($$$) {
    my ($name, $description, $mailhead) = (@_);

    my $sth = $dbh->prepare("SELECT fieldid FROM fielddefs " .
                            "WHERE name = ?");
    $sth->execute($name);
    my ($fieldid) = ($sth->fetchrow_array());
    if (!$fieldid) {
        $dbh->do(q{INSERT INTO fielddefs
                               (name, description, mailhead, sortkey)
                   VALUES (?, ?, ?, ?)},
                 undef, ($name, $description, $mailhead, $headernum));
    } else {
        $dbh->do(q{UPDATE fielddefs
                      SET name = ?, description = ?,
                          mailhead = ?, sortkey = ?
                    WHERE fieldid = ?}, undef,
                 $name, $description, $mailhead, $headernum, $fieldid);
    }
    $headernum++;
}


# Note that all of these entries are unconditional, from when GetFieldID
# used to create an entry if it wasn't found. New fielddef columns should
# be created with their associated schema change.
AddFDef("bug_id", "Bug \#", 1);
AddFDef("short_desc", "Summary", 1);
AddFDef("classification", "Classification", 1);
AddFDef("product", "Product", 1);
AddFDef("version", "Version", 1);
AddFDef("rep_platform", "Platform", 1);
AddFDef("bug_file_loc", "URL", 1);
AddFDef("op_sys", "OS/Version", 1);
AddFDef("bug_status", "Status", 1);
AddFDef("status_whiteboard", "Status Whiteboard", 0);
AddFDef("keywords", "Keywords", 1);
AddFDef("resolution", "Resolution", 0);
AddFDef("bug_severity", "Severity", 1);
AddFDef("priority", "Priority", 1);
AddFDef("component", "Component", 1);
AddFDef("assigned_to", "AssignedTo", 1);
AddFDef("reporter", "ReportedBy", 1);
AddFDef("votes", "Votes", 0);
AddFDef("qa_contact", "QAContact", 1);
AddFDef("cc", "CC", 1);
AddFDef("dependson", "BugsThisDependsOn", 1);
AddFDef("blocked", "OtherBugsDependingOnThis", 1);
AddFDef("attachments.description", "Attachment description", 0);
AddFDef("attachments.thedata", "Attachment data", 0);
AddFDef("attachments.filename", "Attachment filename", 0);
AddFDef("attachments.mimetype", "Attachment mime type", 0);
AddFDef("attachments.ispatch", "Attachment is patch", 0);
AddFDef("attachments.isobsolete", "Attachment is obsolete", 0);
AddFDef("attachments.isprivate", "Attachment is private", 0);

AddFDef("target_milestone", "Target Milestone", 0);
AddFDef("delta_ts", "Last changed date", 0);
AddFDef("longdesc", "Comment", 0);
AddFDef("alias", "Alias", 0);
AddFDef("everconfirmed", "Ever Confirmed", 0);
AddFDef("reporter_accessible", "Reporter Accessible", 0);
AddFDef("cclist_accessible", "CC Accessible", 0);
AddFDef("bug_group", "Group", 0);
AddFDef("estimated_time", "Estimated Hours", 1);
AddFDef("remaining_time", "Remaining Hours", 0);
AddFDef("deadline", "Deadline", 1);
AddFDef("commenter", "Commenter", 0);

# Oops. Bug 163299
$dbh->do("DELETE FROM fielddefs WHERE name='cc_accessible'");

# Oops. Bug 215319
$dbh->do("DELETE FROM fielddefs WHERE name='requesters.login_name'");

AddFDef("flagtypes.name", "Flag", 0);
AddFDef("requestees.login_name", "Flag Requestee", 0);
AddFDef("setters.login_name", "Flag Setter", 0);
AddFDef("work_time", "Hours Worked", 0);
AddFDef("percentage_complete", "Percentage Complete", 0);

AddFDef("content", "Content", 0);

# 2005-11-13 LpSolit@gmail.com - Bug 302599
# One of the field names was a fragment of SQL code, which is DB dependent.
# We have to rename it to a real name, which is DB independent.
my $new_field_name = 'days_elapsed';
my $field_description = 'Days since bug changed';

my ($old_field_id, $old_field_name) =
    $dbh->selectrow_array('SELECT fieldid, name
                           FROM fielddefs
                           WHERE description = ?',
                           undef, $field_description);

if ($old_field_id && ($old_field_name ne $new_field_name)) {
    print "SQL fragment found in the 'fielddefs' table...\n";
    print "Old field name: " . $old_field_name . "\n";
    # We have to fix saved searches first. Queries have been escaped
    # before being saved. We have to do the same here to find them.
    $old_field_name = url_quote($old_field_name);
    my $broken_named_queries =
        $dbh->selectall_arrayref('SELECT userid, name, query
                                  FROM namedqueries WHERE ' .
                                  $dbh->sql_istrcmp('query', '?', 'LIKE'),
                                  undef, "%=$old_field_name%");

    my $sth_UpdateQueries = $dbh->prepare('UPDATE namedqueries SET query = ?
                                           WHERE userid = ? AND name = ?');

    print "Fixing saved searches...\n" if scalar(@$broken_named_queries);
    foreach my $named_query (@$broken_named_queries) {
        my ($userid, $name, $query) = @$named_query;
        $query =~ s/=\Q$old_field_name\E(&|$)/=$new_field_name$1/gi;
        $sth_UpdateQueries->execute($query, $userid, $name);
    }

    # We now do the same with saved chart series.
    my $broken_series =
        $dbh->selectall_arrayref('SELECT series_id, query
                                  FROM series WHERE ' .
                                  $dbh->sql_istrcmp('query', '?', 'LIKE'),
                                  undef, "%=$old_field_name%");

    my $sth_UpdateSeries = $dbh->prepare('UPDATE series SET query = ?
                                          WHERE series_id = ?');

    print "Fixing saved chart series...\n" if scalar(@$broken_series);
    foreach my $series (@$broken_series) {
        my ($series_id, $query) = @$series;
        $query =~ s/=\Q$old_field_name\E(&|$)/=$new_field_name$1/gi;
        $sth_UpdateSeries->execute($query, $series_id);
    }

    # Now that saved searches have been fixed, we can fix the field name.
    print "Fixing the 'fielddefs' table...\n";
    print "New field name: " . $new_field_name . "\n";
    $dbh->do('UPDATE fielddefs SET name = ? WHERE fieldid = ?',
              undef, ($new_field_name, $old_field_id));
}
AddFDef($new_field_name, $field_description, 0);

###########################################################################
# Detect changed local settings
###########################################################################

# Nick Barnes nb+bz@ravenbrook.com 2005-10-05
# 
# PopulateEnumTable($table, @values): if the table $table has no
# entries, fill it with the entries in the list @values, in the same
# order as that list.

sub PopulateEnumTable ($@) {
    my ($table, @valuelist) = @_;

    # If we encounter any of the keys in this hash, they are 
    # automatically set to isactive=0
    my %defaultinactive = ('---' => 1);

    # Check if there are any table entries
    my $query = "SELECT COUNT(id) FROM $table";
    my $sth = $dbh->prepare($query);
    $sth->execute();

    # If the table is empty...
    if ( !$sth->fetchrow_array() ) {
        my $insert = $dbh->prepare("INSERT INTO $table"
            . " (value,sortkey,isactive) VALUES (?,?,?)");
        my $sortorder = 0;
        foreach my $value (@valuelist) {
            $sortorder = $sortorder + 100;
            # Not active if the value exists in $defaultinactive
            my $isactive = exists($defaultinactive{$value}) ? 0 : 1;
            print "Inserting value '$value' in table $table" 
                . " with sortkey $sortorder...\n";
            $insert->execute($value, $sortorder, $isactive);
        }
    }
}

# Set default values for what used to be the enum types.  These values
# are no longer stored in localconfig.  If we are upgrading from a
# Bugzilla with enums to a Bugzilla without enums, we use the
# enum values.
#
# The values that you see here are ONLY DEFAULTS. They are only used
# the FIRST time you run checksetup, IF you are NOT upgrading from a
# Bugzilla with enums. After that, they are either controlled through
# the Bugzilla UI or through the DB.

my $enum_defaults = {
    bug_severity  => ['blocker', 'critical', 'major', 'normal',
                      'minor', 'trivial', 'enhancement'],
    priority     => ["P1","P2","P3","P4","P5"],
    op_sys       => ["All","Windows","Mac OS","Linux","Other"],
    rep_platform => ["All","PC","Macintosh","Other"],
    bug_status   => ["UNCONFIRMED","NEW","ASSIGNED","REOPENED","RESOLVED",
                     "VERIFIED","CLOSED"],
    resolution   => ["","FIXED","INVALID","WONTFIX","LATER","REMIND",
                     "DUPLICATE","WORKSFORME","MOVED"],
};

# Get all the enum column values for the existing database, or the
# defaults if the columns are not enums.
my $enum_values = $dbh->bz_enum_initial_values($enum_defaults);

# Populate the enum tables.
while (my ($table, $values) = each %$enum_values) {
    PopulateEnumTable($table, @$values);
}

###########################################################################
# Create initial test product if there are no products present.
###########################################################################
my $sth = $dbh->prepare("SELECT description FROM products");
$sth->execute;
unless ($sth->rows) {
    print "Creating initial dummy product 'TestProduct' ...\n";
    my $test_product_name = 'TestProduct';
    my $test_product_desc = 
        'This is a test product. This ought to be blown away and'
        . ' replaced with real stuff in a finished installation of bugzilla.';
    my $test_product_version = 'other';

    $dbh->do(q{INSERT INTO products(name, description, milestoneurl, 
                           disallownew, votesperuser, votestoconfirm)
               VALUES (?, ?, '', ?, ?, ?)},
               undef, $test_product_name, $test_product_desc, 0, 0, 0);

    # We could probably just assume that this is "1", but better
    # safe than sorry...
    my $product_id = $dbh->bz_last_key('products', 'id');
    
    $dbh->do(q{INSERT INTO versions (value, product_id) 
                VALUES (?, ?)}, 
             undef, $test_product_version, $product_id);
    # note: since admin user is not yet known, components gets a 0 for 
    # initialowner and this is fixed during final checks.
    $dbh->do("INSERT INTO components (name, product_id, description, " .
                                     "initialowner) " .
             "VALUES (" .
             "'TestComponent', $product_id, " .
             "'This is a test component in the test product database.  " .
             "This ought to be blown away and replaced with real stuff in " .
             "a finished installation of Bugzilla.', 0)");
    $dbh->do(q{INSERT INTO milestones (product_id, value, sortkey) 
               VALUES (?,?,?)},
             undef, $product_id, '---', 0);
}

# Create a default classification if one does not exist
my $class_count =
    $dbh->selectrow_array("SELECT COUNT(*) FROM classifications");
if (!$class_count) {
    $dbh->do("INSERT INTO classifications (name,description) " .
             "VALUES('Unclassified','Unassigned to any classifications')");
}

###########################################################################
# Update the tables to the current definition
###########################################################################

# Both legacy code and modern code need this variable.
my @admins = ();

# really old fields that were added before checksetup.pl existed
# but aren't in very old bugzilla's (like 2.1)
# Steve Stock (sstock@iconnect-inc.com)

$dbh->bz_add_column('bugs', 'target_milestone', 
                    {TYPE => 'varchar(20)', NOTNULL => 1, DEFAULT => "'---'"});
$dbh->bz_add_column('bugs', 'qa_contact', {TYPE => 'INT3'});
$dbh->bz_add_column('bugs', 'status_whiteboard', 
                   {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});
$dbh->bz_add_column('products', 'disallownew', 
                    {TYPE => 'BOOLEAN', NOTNULL => 1}, 0);
$dbh->bz_add_column('products', 'milestoneurl', 
                    {TYPE => 'TINYTEXT', NOTNULL => 1}, '');
$dbh->bz_add_column('components', 'initialqacontact', 
                    {TYPE => 'TINYTEXT'});
$dbh->bz_add_column('components', 'description',
                    {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');

# 1999-06-22 Added an entry to the attachments table to record who the
# submitter was.  Nothing uses this yet, but it still should be recorded.
$dbh->bz_add_column('attachments', 'submitter_id', 
                    {TYPE => 'INT3', NOTNULL => 1}, 0);

#
# One could even populate this field automatically, e.g. with
#
# unless (GetField('attachments', 'submitter_id') {
#    $dbh->bz_add_column ...
#    populate
# }
#
# For now I was too lazy, so you should read the documentation :-)



# 1999-9-15 Apparently, newer alphas of MySQL won't allow you to have "when"
# as a column name.  So, I have had to rename a column in the bugs_activity
# table.

$dbh->bz_rename_column('bugs_activity', 'when', 'bug_when');



# 1999-10-11 Restructured voting database to add a cached value in each bug
# recording how many total votes that bug has.  While I'm at it, I removed
# the unused "area" field from the bugs database.  It is distressing to
# realize that the bugs table has reached the maximum number of indices
# allowed by MySQL (16), which may make future enhancements awkward.
# (P.S. All is not lost; it appears that the latest betas of MySQL support
# a new table format which will allow 32 indices.)

$dbh->bz_drop_column('bugs', 'area');
if (!$dbh->bz_column_info('bugs', 'votes')) {
    $dbh->bz_add_column('bugs', 'votes', {TYPE => 'INT3', NOTNULL => 1,
                                          DEFAULT => 0});
    $dbh->bz_add_index('bugs', 'bugs_votes_idx', [qw(votes)]);
}
$dbh->bz_add_column('products', 'votesperuser', 
                    {TYPE => 'INT2', NOTNULL => 1}, 0);


# The product name used to be very different in various tables.
#
# It was   varchar(16)   in bugs
#          tinytext      in components
#          tinytext      in products
#          tinytext      in versions
#
# tinytext is equivalent to varchar(255), which is quite huge, so I change
# them all to varchar(64).

# Only do this if these fields still exist - they're removed below, in
# a later change
if ($dbh->bz_column_info('products', 'product')) {
    $dbh->bz_alter_column('bugs',       'product', 
                         {TYPE => 'varchar(64)', NOTNULL => 1});
    $dbh->bz_alter_column('components', 'program', {TYPE => 'varchar(64)'});
    $dbh->bz_alter_column('products',   'product', {TYPE => 'varchar(64)'});
    $dbh->bz_alter_column('versions',   'program', 
                          {TYPE => 'varchar(64)', NOTNULL => 1});
}

# 2000-01-16 Added a "keywords" field to the bugs table, which
# contains a string copy of the entries of the keywords table for this
# bug.  This is so that I can easily sort and display a keywords
# column in bug lists.

if (!$dbh->bz_column_info('bugs', 'keywords')) {
    $dbh->bz_add_column('bugs', 'keywords',
                        {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});

    my @kwords;
    print "Making sure 'keywords' field of table 'bugs' is empty ...\n";
    $dbh->do("UPDATE bugs SET keywords = '' " .
              "WHERE keywords != ''");
    print "Repopulating 'keywords' field of table 'bugs' ...\n";
    my $sth = $dbh->prepare("SELECT keywords.bug_id, keyworddefs.name " .
                              "FROM keywords, keyworddefs " .
                             "WHERE keyworddefs.id = keywords.keywordid " .
                          "ORDER BY keywords.bug_id, keyworddefs.name");
    $sth->execute;
    my @list;
    my $bugid = 0;
    my @row;
    while (1) {
        my ($b, $k) = ($sth->fetchrow_array());
        if (!defined $b || $b ne $bugid) {
            if (@list) {
                $dbh->do("UPDATE bugs SET keywords = " .
                         $dbh->quote(join(', ', @list)) .
                         " WHERE bug_id = $bugid");
            }
            if (!$b) {
                last;
            }
            $bugid = $b;
            @list = ();
        }
        push(@list, $k);
    }
}


# 2000-01-18 Added a "disabledtext" field to the profiles table.  If not
# empty, then this account has been disabled, and this field is to contain
# text describing why.
$dbh->bz_add_column('profiles', 'disabledtext',
                    {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');


# 2000-01-20 Added a new "longdescs" table, which is supposed to have all the
# long descriptions in it, replacing the old long_desc field in the bugs 
# table.  The below hideous code populates this new table with things from
# the old field, with ugly parsing and heuristics.

sub WriteOneDesc {
    my ($id, $who, $when, $buffer) = (@_);
    $buffer = trim($buffer);
    if ($buffer eq '') {
        return;
    }
    $dbh->do("INSERT INTO longdescs (bug_id, who, bug_when, thetext) VALUES " .
             "($id, $who, " .  time2str("'%Y/%m/%d %H:%M:%S'", $when) .
             ", " . $dbh->quote($buffer) . ")");
}


if ($dbh->bz_column_info('bugs', 'long_desc')) {
    eval("use Date::Parse");
    eval("use Date::Format");
    my $sth = $dbh->prepare("SELECT count(*) FROM bugs");
    $sth->execute();
    my ($total) = ($sth->fetchrow_array);

    print "Populating new long_desc table.  This is slow.  There are $total\n";
    print "bugs to process; a line of dots will be printed for each 50.\n\n";
    $| = 1;

    $dbh->bz_lock_tables('bugs write', 'longdescs write', 'profiles write',
                         'bz_schema WRITE');

    $dbh->do('DELETE FROM longdescs');

    $sth = $dbh->prepare("SELECT bug_id, creation_ts, reporter, long_desc " .
                           "FROM bugs ORDER BY bug_id");
    $sth->execute();
    my $count = 0;
    while (1) {
        my ($id, $createtime, $reporterid, $desc) = ($sth->fetchrow_array());
        if (!$id) {
            last;
        }
        print ".";
        $count++;
        if ($count % 10 == 0) {
            print " ";
            if ($count % 50 == 0) {
                print "$count/$total (" . int($count * 100 / $total) . "%)\n";
            }
        }
        $desc =~ s/\r//g;
        my $who = $reporterid;
        my $when = str2time($createtime);
        my $buffer = "";
        foreach my $line (split(/\n/, $desc)) {
            $line =~ s/\s+$//g;       # Trim trailing whitespace.
            if ($line =~ /^------- Additional Comments From ([^\s]+)\s+(\d.+\d)\s+-------$/) {
                my $name = $1;
                my $date = str2time($2);
                $date += 59;    # Oy, what a hack.  The creation time is
                                # accurate to the second.  But we the long
                                # text only contains things accurate to the
                                # minute.  And so, if someone makes a comment
                                # within a minute of the original bug creation,
                                # then the comment can come *before* the
                                # bug creation.  So, we add 59 seconds to
                                # the time of all comments, so that they
                                # are always considered to have happened at
                                # the *end* of the given minute, not the
                                # beginning.
                if ($date >= $when) {
                    WriteOneDesc($id, $who, $when, $buffer);
                    $buffer = "";
                    $when = $date;
                    my $s2 = $dbh->prepare("SELECT userid FROM profiles " .
                                            "WHERE login_name = " .
                                           $dbh->quote($name));
                    $s2->execute();
                    ($who) = ($s2->fetchrow_array());
                    if (!$who) {
                        # This username doesn't exist.  Try a special
                        # netscape-only hack (sorry about that, but I don't
                        # think it will hurt any other installations).  We
                        # have many entries in the bugsystem from an ancient
                        # world where the "@netscape.com" part of the loginname
                        # was omitted.  So, look up the user again with that
                        # appended, and use it if it's there.
                        if ($name !~ /\@/) {
                            my $nsname = $name . "\@netscape.com";
                            $s2 =
                                $dbh->prepare("SELECT userid FROM profiles " .
                                               "WHERE login_name = " .
                                              $dbh->quote($nsname));
                            $s2->execute();
                            ($who) = ($s2->fetchrow_array());
                        }
                    }
                            
                    if (!$who) {
                        # This username doesn't exist.  Maybe someone renamed
                        # him or something.  Invent a new profile entry,
                        # disabled, just to represent him.
                        $dbh->do("INSERT INTO profiles " .
                                 "(login_name, cryptpassword," .
                                 " disabledtext) VALUES (" .
                                 $dbh->quote($name) .
                                 ", " . $dbh->quote(bz_crypt('okthen')) . 
                                 ", " . 
                                 "'Account created only to maintain database integrity')");
                        $who = $dbh->bz_last_key('profiles', 'userid');
                    }
                    next;
                } else {
#                    print "\nDecided this line of bug $id has a date of " .
#                        time2str("'%Y/%m/%d %H:%M:%S'", $date) .
#                            "\nwhich is less than previous line:\n$line\n\n";
                }

            }
            $buffer .= $line . "\n";
        }
        WriteOneDesc($id, $who, $when, $buffer);
    }
                

    print "\n\n";

    $dbh->bz_drop_column('bugs', 'long_desc');

    $dbh->bz_unlock_tables();
}


# 2000-01-18 Added a new table fielddefs that records information about the
# different fields we keep an activity log on.  The bugs_activity table
# now has a pointer into that table instead of recording the name directly.

if ($dbh->bz_column_info('bugs_activity', 'field')) {
    $dbh->bz_add_column('bugs_activity', 'fieldid',
                        {TYPE => 'INT3', NOTNULL => 1}, 0);

    $dbh->bz_add_index('bugs_activity', 'bugs_activity_fieldid_idx',
                       [qw(fieldid)]);
    print "Populating new fieldid field ...\n";

    $dbh->bz_lock_tables('bugs_activity WRITE', 'fielddefs WRITE');

    my $sth = $dbh->prepare('SELECT DISTINCT field FROM bugs_activity');
    $sth->execute();
    my %ids;
    while (my ($f) = ($sth->fetchrow_array())) {
        my $q = $dbh->quote($f);
        my $s2 =
            $dbh->prepare("SELECT fieldid FROM fielddefs WHERE name = $q");
        $s2->execute();
        my ($id) = ($s2->fetchrow_array());
        if (!$id) {
            $dbh->do("INSERT INTO fielddefs (name, description) VALUES " .
                     "($q, $q)");
            $id = $dbh->bz_last_key('fielddefs', 'fieldid');
        }
        $dbh->do("UPDATE bugs_activity SET fieldid = $id WHERE field = $q");
    }
    $dbh->bz_unlock_tables();

    $dbh->bz_drop_column('bugs_activity', 'field');
}

        

# 2000-01-18 New email-notification scheme uses a new field in the bug to 
# record when email notifications were last sent about this bug.  Also,
# added 'newemailtech' field to record if user wants to use the experimental
# stuff.
# 2001-04-29 jake@bugzilla.org - The newemailtech field is no longer needed
#   http://bugzilla.mozilla.org/show_bugs.cgi?id=71552

if (!$dbh->bz_column_info('bugs', 'lastdiffed')) {
    $dbh->bz_add_column('bugs', 'lastdiffed', {TYPE =>'DATETIME'});
    $dbh->do('UPDATE bugs SET lastdiffed = now()');
}


# 2000-01-22 The "login_name" field in the "profiles" table was not
# declared to be unique.  Sure enough, somehow, I got 22 duplicated entries
# in my database.  This code detects that, cleans up the duplicates, and
# then tweaks the table to declare the field to be unique.  What a pain.
if (!$dbh->bz_index_info('profiles', 'profiles_login_name_idx')->{TYPE}) {
    print "Searching for duplicate entries in the profiles table ...\n";
    while (1) {
        # This code is weird in that it loops around and keeps doing this
        # select again.  That's because I'm paranoid about deleting entries
        # out from under us in the profiles table.  Things get weird if
        # there are *three* or more entries for the same user...
        $sth = $dbh->prepare("SELECT p1.userid, p2.userid, p1.login_name " .
                               "FROM profiles AS p1, profiles AS p2 " .
                              "WHERE p1.userid < p2.userid " .
                                "AND p1.login_name = p2.login_name " .
                           "ORDER BY p1.login_name");
        $sth->execute();
        my ($u1, $u2, $n) = ($sth->fetchrow_array);
        if (!$u1) {
            last;
        }
        print "Both $u1 & $u2 are ids for $n!  Merging $u2 into $u1 ...\n";
        foreach my $i (["bugs", "reporter"],
                       ["bugs", "assigned_to"],
                       ["bugs", "qa_contact"],
                       ["attachments", "submitter_id"],
                       ["bugs_activity", "who"],
                       ["cc", "who"],
                       ["votes", "who"],
                       ["longdescs", "who"]) {
            my ($table, $field) = (@$i);
            print "   Updating $table.$field ...\n";
            $dbh->do("UPDATE $table SET $field = $u1 " .
                      "WHERE $field = $u2");
        }
        $dbh->do("DELETE FROM profiles WHERE userid = $u2");
    }
    print "OK, changing index type to prevent duplicates in the future ...\n";
    
    $dbh->bz_drop_index('profiles', 'profiles_login_name_idx');
    $dbh->bz_add_index('profiles', 'profiles_login_name_idx',
                       {TYPE => 'UNIQUE', FIELDS => [qw(login_name)]});
}    


# 2000-01-24 Added a new field to let people control whether the "My
# bugs" link appears at the bottom of each page.  Also can control
# whether each named query should show up there.

$dbh->bz_add_column('profiles', 'mybugslink', {TYPE => 'BOOLEAN', NOTNULL => 1,
                                               DEFAULT => 'TRUE'});
$dbh->bz_add_column('namedqueries', 'linkinfooter', 
                    {TYPE => 'BOOLEAN', NOTNULL => 1}, 0);

my $comp_init_owner = $dbh->bz_column_info('components', 'initialowner');
if ($comp_init_owner && $comp_init_owner->{TYPE} eq 'TINYTEXT') {
    $sth = $dbh->prepare(
         "SELECT program, value, initialowner, initialqacontact " .
         "FROM components");
    $sth->execute();
    while (my ($program, $value, $initialowner) = $sth->fetchrow_array()) {
        $initialowner =~ s/([\\\'])/\\$1/g; $initialowner =~ s/\0/\\0/g;
        $program =~ s/([\\\'])/\\$1/g; $program =~ s/\0/\\0/g;
        $value =~ s/([\\\'])/\\$1/g; $value =~ s/\0/\\0/g;

        my $s2 = $dbh->prepare("SELECT userid " .
                                 "FROM profiles " .
                                "WHERE login_name = '$initialowner'");
        $s2->execute();

        my $initialownerid = $s2->fetchrow_array();

        unless (defined $initialownerid) {
            print "Warning: You have an invalid default assignee '$initialowner'\n" .
              "in component '$value' of program '$program'. !\n";
            $initialownerid = 0;
        }

        my $update = 
          "UPDATE components " .
             "SET initialowner = $initialownerid " .
           "WHERE program = '$program' " .
             "AND value = '$value'";
        my $s3 = $dbh->prepare("UPDATE components " .
                                  "SET initialowner = $initialownerid " .
                                "WHERE program = '$program' " .
                                  "AND value = '$value';");
        $s3->execute();
    }

    $dbh->bz_alter_column('components','initialowner',{TYPE => 'INT3'});
}

my $comp_init_qa = $dbh->bz_column_info('components', 'initialqacontact');
if ($comp_init_qa && $comp_init_qa->{TYPE} eq 'TINYTEXT') {
    $sth = $dbh->prepare(
           "SELECT program, value, initialqacontact, initialqacontact " .
           "FROM components");
    $sth->execute();
    while (my ($program, $value, $initialqacontact) = $sth->fetchrow_array()) {
        $initialqacontact =~ s/([\\\'])/\\$1/g; $initialqacontact =~ s/\0/\\0/g;
        $program =~ s/([\\\'])/\\$1/g; $program =~ s/\0/\\0/g;
        $value =~ s/([\\\'])/\\$1/g; $value =~ s/\0/\\0/g;

        my $s2 = $dbh->prepare("SELECT userid " .
                               "FROM profiles " .
                               "WHERE login_name = '$initialqacontact'");
        $s2->execute();

        my $initialqacontactid = $s2->fetchrow_array();

        unless (defined $initialqacontactid) {
            if ($initialqacontact ne '') {
                print "Warning: You have an invalid default QA contact $initialqacontact' in program '$program', component '$value'!\n";
            }
            $initialqacontactid = 0;
        }

        my $update = "UPDATE components " .
            "SET initialqacontact = $initialqacontactid " .
            "WHERE program = '$program' AND value = '$value'";
        my $s3 = $dbh->prepare("UPDATE components " .
                               "SET initialqacontact = $initialqacontactid " .
                               "WHERE program = '$program' " .
                               "AND value = '$value';");
        $s3->execute();
    }

    $dbh->bz_alter_column('components','initialqacontact',{TYPE => 'INT3'});
}


if (!$dbh->bz_column_info('bugs', 'everconfirmed')) {
    $dbh->bz_add_column('bugs', 'everconfirmed',
        {TYPE => 'BOOLEAN', NOTNULL => 1}, 1);
}
$dbh->bz_add_column('products', 'maxvotesperbug',
                    {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '10000'});
$dbh->bz_add_column('products', 'votestoconfirm',
                    {TYPE => 'INT2', NOTNULL => 1}, 0);

# 2000-03-21 Adding a table for target milestones to 
# database - matthew@zeroknowledge.com
# If the milestones table is empty, and we're still back in a Bugzilla
# that has a bugs.product field, that means that we just created
# the milestones table and it needs to be populated.
my $milestones_exist = $dbh->selectrow_array("SELECT 1 FROM milestones");
if (!$milestones_exist && $dbh->bz_column_info('bugs', 'product')) {
    print "Replacing blank milestones...\n";

    $dbh->do("UPDATE bugs " .
             "SET target_milestone = '---' " .
             "WHERE target_milestone = ' '");

    # If we are upgrading from 2.8 or earlier, we will have *created*
    # the milestones table with a product_id field, but Bugzilla expects
    # it to have a "product" field. So we change the field backward so
    # other code can run. The change will be reversed later in checksetup.
    if ($dbh->bz_column_info('milestones', 'product_id')) {
        # Dropping the column leaves us with a milestones_product_id_idx
        # index that is only on the "value" column. We need to drop the
        # whole index so that it can be correctly re-created later.
        $dbh->bz_drop_index('milestones', 'milestones_product_id_idx');
        $dbh->bz_drop_column('milestones', 'product_id');
        $dbh->bz_add_column('milestones', 'product', 
            {TYPE => 'varchar(64)', NOTNULL => 1}, '');
    }

    # Populate the milestone table with all existing values in the database
    $sth = $dbh->prepare("SELECT DISTINCT target_milestone, product FROM bugs");
    $sth->execute();
    
    print "Populating milestones table...\n";
    
    my $value;
    my $product;
    while(($value, $product) = $sth->fetchrow_array())
    {
        # check if the value already exists
        my $sortkey = substr($value, 1);
        if ($sortkey !~ /^\d+$/) {
            $sortkey = 0;
        } else {
            $sortkey *= 10;
        }
        $value = $dbh->quote($value);
        $product = $dbh->quote($product);
        my $s2 = $dbh->prepare("SELECT value " .
                               "FROM milestones " .
                               "WHERE value = $value " .
                               "AND product = $product");
        $s2->execute();
        
        if(!$s2->fetchrow_array())
        {
            $dbh->do("INSERT INTO milestones(value, product, sortkey) VALUES($value, $product, $sortkey)");
        }
    }
}

# 2000-03-22 Changed the default value for target_milestone to be "---"
# (which is still not quite correct, but much better than what it was 
# doing), and made the size of the value field in the milestones table match
# the size of the target_milestone field in the bugs table.

$dbh->bz_alter_column('bugs', 'target_milestone',
    {TYPE => 'varchar(20)', NOTNULL => 1, DEFAULT => "'---'"});
$dbh->bz_alter_column('milestones', 'value', 
    {TYPE => 'varchar(20)', NOTNULL => 1});


# 2000-03-23 Added a defaultmilestone field to the products table, so that
# we know which milestone to initially assign bugs to.

if (!$dbh->bz_column_info('products', 'defaultmilestone')) {
    $dbh->bz_add_column('products', 'defaultmilestone',
             {TYPE => 'varchar(20)', NOTNULL => 1, DEFAULT => "'---'"});
    $sth = $dbh->prepare("SELECT product, defaultmilestone FROM products");
    $sth->execute();
    while (my ($product, $defaultmilestone) = $sth->fetchrow_array()) {
        $product = $dbh->quote($product);
        $defaultmilestone = $dbh->quote($defaultmilestone);
        my $s2 = $dbh->prepare("SELECT value FROM milestones " .
                               "WHERE value = $defaultmilestone " .
                               "AND product = $product");
        $s2->execute();
        if (!$s2->fetchrow_array()) {
            $dbh->do("INSERT INTO milestones(value, product) " .
                     "VALUES ($defaultmilestone, $product)");
        }
    }
}

# 2000-03-24 Added unique indexes into the cc and keyword tables.  This
# prevents certain database inconsistencies, and, moreover, is required for
# new generalized list code to work.

if (!$dbh->bz_index_info('cc', 'cc_bug_id_idx')->{TYPE}) {

    # XXX should eliminate duplicate entries before altering
    #
    $dbh->bz_drop_index('cc', 'cc_bug_id_idx');
    $dbh->bz_add_index('cc', 'cc_bug_id_idx', 
                      {TYPE => 'UNIQUE', FIELDS => [qw(bug_id who)]});
}    

if (!$dbh->bz_index_info('keywords', 'keywords_bug_id_idx')->{TYPE}) {

    # XXX should eliminate duplicate entries before altering
    #
    $dbh->bz_drop_index('keywords', 'keywords_bug_id_idx');
    $dbh->bz_add_index('keywords', 'keywords_bug_id_idx', 
                      {TYPE => 'UNIQUE', FIELDS => [qw(bug_id keywordid)]});
}    

# 2000-07-15 Added duplicates table so Bugzilla tracks duplicates in a better 
# way than it used to. This code searches the comments to populate the table
# initially. It's executed if the table is empty; if it's empty because there
# are no dupes (as opposed to having just created the table) it won't have
# any effect anyway, so it doesn't matter.
$sth = $dbh->prepare("SELECT count(*) from duplicates");
$sth->execute();
if (!($sth->fetchrow_arrayref()->[0])) {
    # populate table
    print("Populating duplicates table...\n") unless $silent;
    
    $sth = $dbh->prepare(
        "SELECT longdescs.bug_id, thetext " .
          "FROM longdescs " .
     "LEFT JOIN bugs using(bug_id) " .
         "WHERE (thetext " . $dbh->sql_regexp .
                 " '[.*.]{3} This bug has been marked as a duplicate of [[:digit:]]+ [.*.]{3}') " .
           "AND (resolution = 'DUPLICATE') " .
      "ORDER BY longdescs.bug_when");
    $sth->execute();

    my %dupes;
    my $key;

    # Because of the way hashes work, this loop removes all but the last dupe
    # resolution found for a given bug.
    while (my ($dupe, $dupe_of) = $sth->fetchrow_array()) {
        $dupes{$dupe} = $dupe_of;
    }

    foreach $key (keys(%dupes)){
        $dupes{$key} =~ /^.*\*\*\* This bug has been marked as a duplicate of (\d+) \*\*\*$/ms;
        $dupes{$key} = $1;
        $dbh->do("INSERT INTO duplicates VALUES('$dupes{$key}', '$key')");
        #                                        BugItsADupeOf   Dupe
    }
}

# 2000-12-18.  Added an 'emailflags' field for storing preferences about
# when email gets sent on a per-user basis.
if (!$dbh->bz_column_info('profiles', 'emailflags') && 
    !$dbh->bz_column_info('email_setting', 'user_id')) {
    $dbh->bz_add_column('profiles', 'emailflags', {TYPE => 'MEDIUMTEXT'});
}

# 2000-11-27 For Bugzilla 2.5 and later. Copy data from 'comments' to
# 'longdescs' - the new name of the comments table.
if ($dbh->bz_table_info('comments')) {
    my $quoted_when = $dbh->quote_identifier('when');
    # This is MySQL-specific syntax, but that's OK because it will only
    # ever run on MySQL.
    $dbh->do("INSERT INTO longdescs (bug_when, bug_id, who, thetext)
              SELECT $quoted_when, bug_id, who, comment
                FROM comments");
    $dbh->bz_drop_table("comments");
}

# 2001-04-08 Added a special directory for the duplicates stats.
unless (-d "$datadir/duplicates") {
    print "Creating duplicates directory...\n";
    mkdir "$datadir/duplicates", 0770; 
    if ($my_webservergroup eq "") {
        chmod 01777, "$datadir/duplicates";
    } 
}

#
# 2001-04-10 myk@mozilla.org:
# isactive determines whether or not a group is active.  An inactive group
# cannot have bugs added to it.  Deactivation is a much milder form of
# deleting a group that allows users to continue to work on bugs in the group
# without enabling them to extend the life of the group by adding bugs to it.
# http://bugzilla.mozilla.org/show_bug.cgi?id=75482
#
$dbh->bz_add_column('groups', 'isactive', 
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

#
# 2001-06-15 myk@mozilla.org:
# isobsolete determines whether or not an attachment is pertinent/relevant/valid.
#
$dbh->bz_add_column('attachments', 'isobsolete', 
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

# 2001-04-29 jake@bugzilla.org - Remove oldemailtech
#   http://bugzilla.mozilla.org/show_bugs.cgi?id=71552
if (-d 'shadow') {
    print "Removing shadow directory...\n";
    unlink glob("shadow/*");
    unlink glob("shadow/.*");
    rmdir "shadow";
}
$dbh->bz_drop_column("profiles", "emailnotification");
$dbh->bz_drop_column("profiles", "newemailtech");


# 2003-11-19; chicks@chicks.net; bug 225973: fix field size to accomodate
# wider algorithms such as Blowfish. Note that this needs to be run
# before recrypting passwords in the following block.
$dbh->bz_alter_column('profiles', 'cryptpassword', {TYPE => 'varchar(128)'});

# 2001-06-12; myk@mozilla.org; bugs 74032, 77473:
# Recrypt passwords using Perl &crypt instead of the mysql equivalent
# and delete plaintext passwords from the database.
if ($dbh->bz_column_info('profiles', 'password')) {
    
    print <<ENDTEXT;
Your current installation of Bugzilla stores passwords in plaintext 
in the database and uses mysql's encrypt function instead of Perl's 
crypt function to crypt passwords.  Passwords are now going to be 
re-crypted with the Perl function, and plaintext passwords will be 
deleted from the database.  This could take a while if your  
installation has many users. 
ENDTEXT

    # Re-crypt everyone's password.
    my $sth = $dbh->prepare("SELECT userid, password FROM profiles");
    $sth->execute();

    my $i = 1;

    print "Fixing password #1... ";
    while (my ($userid, $password) = $sth->fetchrow_array()) {
        my $cryptpassword = $dbh->quote(bz_crypt($password));
        $dbh->do("UPDATE profiles " .
                    "SET cryptpassword = $cryptpassword " .
                  "WHERE userid = $userid");
        ++$i;
        # Let the user know where we are at every 500 records.
        print "$i... " if !($i%500);
    }
    print "$i... Done.\n";

    # Drop the plaintext password field.
    $dbh->bz_drop_column('profiles', 'password');
}

#
# 2001-06-06 justdave@syndicomm.com:
# There was no index on the 'who' column in the long descriptions table.
# This caused queries by who posted comments to take a LONG time.
#   http://bugzilla.mozilla.org/show_bug.cgi?id=57350
$dbh->bz_add_index('longdescs', 'longdescs_who_idx', [qw(who)]);

# 2001-06-15 kiko@async.com.br - Change bug:version size to avoid
# truncates re http://bugzilla.mozilla.org/show_bug.cgi?id=9352
$dbh->bz_alter_column('bugs', 'version', 
                      {TYPE => 'varchar(64)', NOTNULL => 1});

# 2001-07-20 jake@bugzilla.org - Change bugs_activity to only record changes
#  http://bugzilla.mozilla.org/show_bug.cgi?id=55161
if ($dbh->bz_column_info('bugs_activity', 'oldvalue')) {
    $dbh->bz_add_column("bugs_activity", "removed", {TYPE => "TINYTEXT"});
    $dbh->bz_add_column("bugs_activity", "added", {TYPE => "TINYTEXT"});

    # Need to get fieldid's for the fields that have multiple values
    my @multi = ();
    foreach my $f ("cc", "dependson", "blocked", "keywords") {
        my $sth = $dbh->prepare("SELECT fieldid " .
                                "FROM fielddefs " .
                                "WHERE name = '$f'");
        $sth->execute();
        my ($fid) = $sth->fetchrow_array();
        push (@multi, $fid);
    } 

    # Now we need to process the bugs_activity table and reformat the data
    my $i = 0;
    print "Fixing activity log ";
    my $sth = $dbh->prepare("SELECT bug_id, who, bug_when, fieldid,
                            oldvalue, newvalue FROM bugs_activity");
    $sth->execute;
    while (my ($bug_id, $who, $bug_when, $fieldid, $oldvalue, $newvalue) = $sth->fetchrow_array()) {
        # print the iteration count every 500 records 
        # so the user knows we didn't die
        print "$i..." if !($i++ % 500); 
        # Make sure (old|new)value isn't null (to suppress warnings)
        $oldvalue ||= "";
        $newvalue ||= "";
        my ($added, $removed) = "";
        if (grep ($_ eq $fieldid, @multi)) {
            $oldvalue =~ s/[\s,]+/ /g;
            $newvalue =~ s/[\s,]+/ /g;
            my @old = split(" ", $oldvalue);
            my @new = split(" ", $newvalue);
            my (@add, @remove) = ();
            # Find values that were "added"
            foreach my $value(@new) {
                if (! grep ($_ eq $value, @old)) {
                    push (@add, $value);
                }
            }
            # Find values that were removed
            foreach my $value(@old) {
                if (! grep ($_ eq $value, @new)) {
                    push (@remove, $value);
                }
            }
            $added = join (", ", @add);
            $removed = join (", ", @remove);
            # If we can't determine what changed, put a ? in both fields
            unless ($added || $removed) {
                $added = "?";
                $removed = "?";
            }
            # If the original field (old|new)value was full, then this
            # could be incomplete data.
            if (length($oldvalue) == 255 || length($newvalue) == 255) {
                $added = "? $added";
                $removed = "? $removed";
            }
        } else {
            $removed = $oldvalue;
            $added = $newvalue;
        }
        $added = $dbh->quote($added);
        $removed = $dbh->quote($removed);
        $dbh->do("UPDATE bugs_activity SET removed = $removed, added = $added
                  WHERE bug_id = $bug_id AND who = $who
                   AND bug_when = '$bug_when' AND fieldid = $fieldid");
    }
    print ". Done.\n";
    $dbh->bz_drop_column("bugs_activity", "oldvalue");
    $dbh->bz_drop_column("bugs_activity", "newvalue");
} 

$dbh->bz_alter_column("profiles", "disabledtext", 
                      {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');

# 2001-07-26 myk@mozilla.org            bug 39816 (original)
# 2002-02-06 bbaetz@student.usyd.edu.au bug 97471 (revision)
# Add fields to the bugs table that record whether or not the reporter
# and users on the cc: list can see bugs even when
# they are not members of groups to which the bugs are restricted.
$dbh->bz_add_column("bugs", "reporter_accessible", 
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});
$dbh->bz_add_column("bugs", "cclist_accessible", 
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

# 2001-08-21 myk@mozilla.org bug84338:
# Add a field to the bugs_activity table for the attachment ID, so installations
# using the attachment manager can record changes to attachments.
$dbh->bz_add_column("bugs_activity", "attach_id", {TYPE => 'INT3'});

# 2002-02-04 bbaetz@student.usyd.edu.au bug 95732
# Remove logincookies.cryptpassword, and delete entries which become
# invalid
if ($dbh->bz_column_info("logincookies", "cryptpassword")) {
    # We need to delete any cookies which are invalid before dropping the
    # column

    print "Removing invalid login cookies...\n";

    # mysql doesn't support DELETE with multi-table queries, so we have
    # to iterate
    my $sth = $dbh->prepare("SELECT cookie FROM logincookies, profiles " .
                            "WHERE logincookies.cryptpassword != " .
                            "profiles.cryptpassword AND " .
                            "logincookies.userid = profiles.userid");
    $sth->execute();
    while (my ($cookie) = $sth->fetchrow_array()) {
        $dbh->do("DELETE FROM logincookies WHERE cookie = $cookie");
    }

    $dbh->bz_drop_column("logincookies", "cryptpassword");
}

# 2002-02-13 bbaetz@student.usyd.edu.au - bug 97471
# qacontact/assignee should always be able to see bugs,
# so remove their restriction column
if ($dbh->bz_column_info("bugs", "qacontact_accessible")) {
    print "Removing restrictions on bugs for assignee and qacontact...\n";

    $dbh->bz_drop_column("bugs", "qacontact_accessible");
    $dbh->bz_drop_column("bugs", "assignee_accessible");
}

# 2002-02-20 jeff.hedlund@matrixsi.com - bug 24789 time tracking
$dbh->bz_add_column("longdescs", "work_time", 
                    {TYPE => 'decimal(5,2)', NOTNULL => 1, DEFAULT => '0'});
$dbh->bz_add_column("bugs", "estimated_time", 
                    {TYPE => 'decimal(5,2)', NOTNULL => 1, DEFAULT => '0'});
$dbh->bz_add_column("bugs", "remaining_time",
                    {TYPE => 'decimal(5,2)', NOTNULL => 1, DEFAULT => '0'});
$dbh->bz_add_column("bugs", "deadline", {TYPE => 'DATETIME'});

# 2002-03-15 bbaetz@student.usyd.edu.au - bug 129466
# 2002-05-13 preed@sigkill.com - bug 129446 patch backported to the 
#  BUGZILLA-2_14_1-BRANCH as a security blocker for the 2.14.2 release
# 
# Use the ip, not the hostname, in the logincookies table
if ($dbh->bz_column_info("logincookies", "hostname")) {
    # We've changed what we match against, so all entries are now invalid
    $dbh->do("DELETE FROM logincookies");

    # Now update the logincookies schema
    $dbh->bz_drop_column("logincookies", "hostname");
    $dbh->bz_add_column("logincookies", "ipaddr", 
                        {TYPE => 'varchar(40)', NOTNULL => 1}, '');
}

# 2002-08-19 - bugreport@peshkin.net bug 143826
# Add private comments and private attachments on less-private bugs
$dbh->bz_add_column('longdescs', 'isprivate', 
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
$dbh->bz_add_column('attachments', 'isprivate',
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});


# 2002-07-03 myk@mozilla.org bug99203:
# Add a bug alias field to the bugs table so bugs can be referenced by alias
# in addition to ID.
if (!$dbh->bz_column_info("bugs", "alias")) {
    $dbh->bz_add_column("bugs", "alias", {TYPE => "varchar(20)"});
    $dbh->bz_add_index('bugs', 'bugs_alias_idx', 
                       {TYPE => 'UNIQUE', FIELDS => [qw(alias)]});
}

# 2002-07-15 davef@tetsubo.com - bug 67950
# Move quips to the db.
if (-r "$datadir/comments" && -s "$datadir/comments"
    && open (COMMENTS, "<$datadir/comments")) {
    print "Populating quips table from $datadir/comments...\n\n";
    while (<COMMENTS>) {
        chomp;
        $dbh->do("INSERT INTO quips (quip) VALUES ("
                 . $dbh->quote($_) . ")");
    }
    print "Quips are now stored in the database, rather than in an external file.\n" .
          "The quips previously stored in $datadir/comments have been copied into\n" .
          "the database, and that file has been renamed to $datadir/comments.bak\n"  .
          "You may delete the renamed file once you have confirmed that all your \n" .
          "quips were moved successfully.\n\n";
    close COMMENTS;
    rename("$datadir/comments", "$datadir/comments.bak");
}

# 2002-07-31 bbaetz@student.usyd.edu.au bug 158236
# Remove unused column
if ($dbh->bz_column_info("namedqueries", "watchfordiffs")) {
    $dbh->bz_drop_column("namedqueries", "watchfordiffs");
}

# 2002-08-12 jake@bugzilla.org/bbaetz@student.usyd.edu.au - bug 43600
# Use integer IDs for products and components.
if ($dbh->bz_column_info("products", "product")) {
    print "Updating database to use product IDs.\n";

    # First, we need to remove possible NULL entries
    # NULLs may exist, but won't have been used, since all the uses of them
    # are in NOT NULL fields in other tables
    $dbh->do("DELETE FROM products WHERE product IS NULL");
    $dbh->do("DELETE FROM components WHERE value IS NULL");

    $dbh->bz_add_column("products", "id", 
        {TYPE => 'SMALLSERIAL', NOTNULL => 1, PRIMARYKEY => 1});
    $dbh->bz_add_column("components", "product_id", 
        {TYPE => 'INT2', NOTNULL => 1}, 0);
    $dbh->bz_add_column("versions", "product_id", 
        {TYPE => 'INT2', NOTNULL => 1}, 0);
    $dbh->bz_add_column("milestones", "product_id", 
        {TYPE => 'INT2', NOTNULL => 1}, 0);
    $dbh->bz_add_column("bugs", "product_id",
        {TYPE => 'INT2', NOTNULL => 1}, 0);

    # The attachstatusdefs table was added in version 2.15, but removed again
    # in early 2.17.  If it exists now, we still need to perform this change
    # with product_id because the code further down which converts the
    # attachment statuses to flags depends on it.  But we need to avoid this
    # if the user is upgrading from 2.14 or earlier (because it won't be
    # there to convert).
    if ($dbh->bz_table_info("attachstatusdefs")) {
        $dbh->bz_add_column("attachstatusdefs", "product_id", 
            {TYPE => 'INT2', NOTNULL => 1}, 0);
    }

    my %products;
    my $sth = $dbh->prepare("SELECT id, product FROM products");
    $sth->execute;
    while (my ($product_id, $product) = $sth->fetchrow_array()) {
        if (exists $products{$product}) {
            print "Ignoring duplicate product $product\n";
            $dbh->do("DELETE FROM products WHERE id = $product_id");
            next;
        }
        $products{$product} = 1;
        $dbh->do("UPDATE components SET product_id = $product_id " .
                 "WHERE program = " . $dbh->quote($product));
        $dbh->do("UPDATE versions SET product_id = $product_id " .
                 "WHERE program = " . $dbh->quote($product));
        $dbh->do("UPDATE milestones SET product_id = $product_id " .
                 "WHERE product = " . $dbh->quote($product));
        $dbh->do("UPDATE bugs SET product_id = $product_id " .
                 "WHERE product = " . $dbh->quote($product));
        $dbh->do("UPDATE attachstatusdefs SET product_id = $product_id " .
                 "WHERE product = " . $dbh->quote($product)) 
            if $dbh->bz_table_info("attachstatusdefs");
    }

    print "Updating the database to use component IDs.\n";
    $dbh->bz_add_column("components", "id", 
        {TYPE => 'SMALLSERIAL', NOTNULL => 1, PRIMARYKEY => 1});
    $dbh->bz_add_column("bugs", "component_id",
                        {TYPE => 'INT2', NOTNULL => 1}, 0);

    my %components;
    $sth = $dbh->prepare("SELECT id, value, product_id FROM components");
    $sth->execute;
    while (my ($component_id, $component, $product_id) = $sth->fetchrow_array()) {
        if (exists $components{$component}) {
            if (exists $components{$component}{$product_id}) {
                print "Ignoring duplicate component $component for product $product_id\n";
                $dbh->do("DELETE FROM components WHERE id = $component_id");
                next;
            }
        } else {
            $components{$component} = {};
        }
        $components{$component}{$product_id} = 1;
        $dbh->do("UPDATE bugs SET component_id = $component_id " .
                  "WHERE component = " . $dbh->quote($component) .
                   " AND product_id = $product_id");
    }
    print "Fixing Indexes and Uniqueness.\n";
    $dbh->bz_drop_index('milestones', 'milestones_product_idx');

    $dbh->bz_add_index('milestones', 'milestones_product_id_idx',
        {TYPE => 'UNIQUE', FIELDS => [qw(product_id value)]});

    $dbh->bz_drop_index('bugs', 'bugs_product_idx');
    $dbh->bz_add_index('bugs', 'bugs_product_id_idx', [qw(product_id)]);
    $dbh->bz_drop_index('bugs', 'bugs_component_idx');
    $dbh->bz_add_index('bugs', 'bugs_component_id_idx', [qw(component_id)]);

    print "Removing, renaming, and retyping old product and component fields.\n";
    $dbh->bz_drop_column("components", "program");
    $dbh->bz_drop_column("versions", "program");
    $dbh->bz_drop_column("milestones", "product");
    $dbh->bz_drop_column("bugs", "product");
    $dbh->bz_drop_column("bugs", "component");
    $dbh->bz_drop_column("attachstatusdefs", "product")
        if $dbh->bz_table_info("attachstatusdefs");
    $dbh->bz_rename_column("products", "product", "name");
    $dbh->bz_alter_column("products", "name",
                          {TYPE => 'varchar(64)', NOTNULL => 1});
    $dbh->bz_rename_column("components", "value", "name");
    $dbh->bz_alter_column("components", "name",
                          {TYPE => 'varchar(64)', NOTNULL => 1});

    print "Adding indexes for products and components tables.\n";
    $dbh->bz_add_index('products', 'products_name_idx',
                       {TYPE => 'UNIQUE', FIELDS => [qw(name)]});
    $dbh->bz_add_index('components', 'components_product_id_idx',
        {TYPE => 'UNIQUE', FIELDS => [qw(product_id name)]});
    $dbh->bz_add_index('components', 'components_name_idx', [qw(name)]);
}

# 2002-09-22 - bugreport@peshkin.net - bug 157756
#
# If the whole groups system is new, but the installation isn't, 
# convert all the old groupset groups, etc...
#
# This requires:
# 1) define groups ids in group table
# 2) populate user_group_map with grants from old groupsets and blessgroupsets
# 3) populate bug_group_map with data converted from old bug groupsets
# 4) convert activity logs to use group names instead of numbers
# 5) identify the admin from the old all-ones groupset
#
# ListBits(arg) returns a list of UNKNOWN<n> if the group
# has been deleted for all bits set in arg. When the activity
# records are converted from groupset numbers to lists of
# group names, ListBits is used to fill in a list of references
# to groupset bits for groups that no longer exist.
# 
sub ListBits {
    my ($num) = @_;
    my @res = ();
    my $curr = 1;
    while (1) {
        # Convert a big integer to a list of bits 
        my $sth = $dbh->prepare("SELECT ($num & ~$curr) > 0, 
                                        ($num & $curr), 
                                        ($num & ~$curr), 
                                        $curr << 1");
        $sth->execute;
        my ($more, $thisbit, $remain, $nval) = $sth->fetchrow_array;
        push @res,"UNKNOWN<$curr>" if ($thisbit);
        $curr = $nval;
        $num = $remain;
        last if (!$more);
    }
    return @res;
}

# The groups system needs to be converted if groupset exists
if ($dbh->bz_column_info("profiles", "groupset")) {
    $dbh->bz_add_column('groups', 'last_changed', 
        {TYPE => 'DATETIME', NOTNULL => 1}, '0000-00-00 00:00:00');

    # Some mysql versions will promote any unique key to primary key
    # so all unique keys are removed first and then added back in
    $dbh->bz_drop_index('groups', 'groups_bit_idx');
    $dbh->bz_drop_index('groups', 'groups_name_idx');
    if ($dbh->primary_key(undef, undef, 'groups')) {
        $dbh->do("ALTER TABLE groups DROP PRIMARY KEY");
    }

    $dbh->bz_add_column('groups', 'id',
        {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

    $dbh->bz_add_index('groups', 'groups_name_idx', 
                       {TYPE => 'UNIQUE', FIELDS => [qw(name)]});
    $dbh->bz_add_column('profiles', 'refreshed_when',
        {TYPE => 'DATETIME', NOTNULL => 1}, '0000-00-00 00:00:00');

    # Convert all existing groupset records to map entries before removing
    # groupset fields or removing "bit" from groups.
    $sth = $dbh->prepare("SELECT bit, id FROM groups
                WHERE bit > 0");
    $sth->execute();
    while (my ($bit, $gid) = $sth->fetchrow_array) {
        # Create user_group_map membership grants for old groupsets.
        # Get each user with the old groupset bit set
        my $sth2 = $dbh->prepare("SELECT userid FROM profiles
                   WHERE (groupset & $bit) != 0");
        $sth2->execute();
        while (my ($uid) = $sth2->fetchrow_array) {
            # Check to see if the user is already a member of the group
            # and, if not, insert a new record.
            my $query = "SELECT user_id FROM user_group_map 
                WHERE group_id = $gid AND user_id = $uid 
                AND isbless = 0"; 
            my $sth3 = $dbh->prepare($query);
            $sth3->execute();
            if ( !$sth3->fetchrow_array() ) {
                $dbh->do("INSERT INTO user_group_map
                       (user_id, group_id, isbless, grant_type)
                       VALUES($uid, $gid, 0, " . GRANT_DIRECT . ")");
            }
        }
        # Create user can bless group grants for old groupsets, but only
        # if we're upgrading from a Bugzilla that had blessing.
        if($dbh->bz_column_info('profiles', 'blessgroupset')) {
            # Get each user with the old blessgroupset bit set
            $sth2 = $dbh->prepare("SELECT userid FROM profiles
                       WHERE (blessgroupset & $bit) != 0");
            $sth2->execute();
            while (my ($uid) = $sth2->fetchrow_array) {
                $dbh->do("INSERT INTO user_group_map
                       (user_id, group_id, isbless, grant_type)
                       VALUES($uid, $gid, 1, " . GRANT_DIRECT . ")");
            }
        }
        # Create bug_group_map records for old groupsets.
        # Get each bug with the old group bit set.
        $sth2 = $dbh->prepare("SELECT bug_id FROM bugs
                   WHERE (groupset & $bit) != 0");
        $sth2->execute();
        while (my ($bug_id) = $sth2->fetchrow_array) {
            # Insert the bug, group pair into the bug_group_map.
            $dbh->do("INSERT INTO bug_group_map
                   (bug_id, group_id)
                   VALUES($bug_id, $gid)");
        }
    }
    # Replace old activity log groupset records with lists of names of groups.
    # Start by defining the bug_group field and getting its id.
    AddFDef("bug_group", "Group", 0);
    $sth = $dbh->prepare("SELECT fieldid " .
                           "FROM fielddefs " .
                          "WHERE name = " . $dbh->quote('bug_group'));
    $sth->execute();
    my ($bgfid) = $sth->fetchrow_array;
    # Get the field id for the old groupset field
    $sth = $dbh->prepare("SELECT fieldid " .
                           "FROM fielddefs " .
                          "WHERE name = " . $dbh->quote('groupset'));
    $sth->execute();
    my ($gsid) = $sth->fetchrow_array;
    # Get all bugs_activity records from groupset changes
    if ($gsid) {
        $sth = $dbh->prepare("SELECT bug_id, bug_when, who, added, removed
                              FROM bugs_activity WHERE fieldid = $gsid");
        $sth->execute();
        while (my ($bug_id, $bug_when, $who, $added, $removed) = $sth->fetchrow_array) {
            $added ||= 0;
            $removed ||= 0;
            # Get names of groups added.
            my $sth2 = $dbh->prepare("SELECT name " .
                                       "FROM groups " .
                                      "WHERE (bit & $added) != 0 " .
                                        "AND (bit & $removed) = 0");
            $sth2->execute();
            my @logadd = ();
            while (my ($n) = $sth2->fetchrow_array) {
                push @logadd, $n;
            }
            # Get names of groups removed.
            $sth2 = $dbh->prepare("SELECT name " .
                                    "FROM groups " .
                                   "WHERE (bit & $removed) != 0 " .
                                     "AND (bit & $added) = 0");
            $sth2->execute();
            my @logrem = ();
            while (my ($n) = $sth2->fetchrow_array) {
                push @logrem, $n;
            }
            # Get list of group bits added that correspond to missing groups.
            $sth2 = $dbh->prepare("SELECT ($added & ~BIT_OR(bit)) FROM groups");
            $sth2->execute();
            my ($miss) = $sth2->fetchrow_array;
            if ($miss) {
                push @logadd, ListBits($miss);
                print "\nWARNING - GROUPSET ACTIVITY ON BUG $bug_id CONTAINS DELETED GROUPS\n";
            }
            # Get list of group bits deleted that correspond to missing groups.
            $sth2 = $dbh->prepare("SELECT ($removed & ~BIT_OR(bit)) FROM groups");
            $sth2->execute();
            ($miss) = $sth2->fetchrow_array;
            if ($miss) {
                push @logrem, ListBits($miss);
                print "\nWARNING - GROUPSET ACTIVITY ON BUG $bug_id CONTAINS DELETED GROUPS\n";
            }
            my $logr = "";
            my $loga = "";
            $logr = join(", ", @logrem) . '?' if @logrem;
            $loga = join(", ", @logadd) . '?' if @logadd;
            # Replace to old activity record with the converted data.
            $dbh->do("UPDATE bugs_activity SET fieldid = $bgfid, added = " .
                      $dbh->quote($loga) . ", removed = " . 
                      $dbh->quote($logr) .
                     " WHERE bug_id = $bug_id AND bug_when = " . 
                      $dbh->quote($bug_when) .
                     " AND who = $who AND fieldid = $gsid");
    
        }
        # Replace groupset changes with group name changes in profiles_activity.
        # Get profiles_activity records for groupset.
        $sth = $dbh->prepare(
             "SELECT userid, profiles_when, who, newvalue, oldvalue " .
               "FROM profiles_activity " .
              "WHERE fieldid = $gsid");
        $sth->execute();
        while (my ($uid, $uwhen, $uwho, $added, $removed) = $sth->fetchrow_array) {
            $added ||= 0;
            $removed ||= 0;
            # Get names of groups added.
            my $sth2 = $dbh->prepare("SELECT name " .
                                       "FROM groups " .
                                      "WHERE (bit & $added) != 0 " .
                                        "AND (bit & $removed) = 0");
            $sth2->execute();
            my @logadd = ();
            while (my ($n) = $sth2->fetchrow_array) {
                push @logadd, $n;
            }
            # Get names of groups removed.
            $sth2 = $dbh->prepare("SELECT name " .
                                    "FROM groups " .
                                   "WHERE (bit & $removed) != 0 " .
                                     "AND (bit & $added) = 0");
            $sth2->execute();
            my @logrem = ();
            while (my ($n) = $sth2->fetchrow_array) {
                push @logrem, $n;
            }
            my $ladd = "";
            my $lrem = "";
            $ladd = join(", ", @logadd) . '?' if @logadd;
            $lrem = join(", ", @logrem) . '?' if @logrem;
            # Replace profiles_activity record for groupset change 
            # with group list.
            $dbh->do("UPDATE profiles_activity " .
                     "SET fieldid = $bgfid, newvalue = " .
                      $dbh->quote($ladd) . ", oldvalue = " . 
                      $dbh->quote($lrem) .
                      " WHERE userid = $uid AND profiles_when = " . 
                      $dbh->quote($uwhen) .
                      " AND who = $uwho AND fieldid = $gsid");
    
        }
    }
    # Identify admin group.
    my $sth = $dbh->prepare("SELECT id FROM groups 
                WHERE name = 'admin'");
    $sth->execute();
    my ($adminid) = $sth->fetchrow_array();
    # find existing admins
    # Don't lose admins from DBs where Bug 157704 applies
    $sth = $dbh->prepare(
           "SELECT userid, (groupset & 65536), login_name " .
             "FROM profiles " .
            "WHERE (groupset | 65536) = 9223372036854775807");
    $sth->execute();
    while ( my ($userid, $iscomplete, $login_name) = $sth->fetchrow_array() ) {
        # existing administrators are made members of group "admin"
        print "\nWARNING - $login_name IS AN ADMIN IN SPITE OF BUG 157704\n\n"
            if (!$iscomplete);
        push @admins, $userid;
    }
    $dbh->bz_drop_column('profiles','groupset');
    $dbh->bz_drop_column('profiles','blessgroupset');
    $dbh->bz_drop_column('bugs','groupset');
    $dbh->bz_drop_column('groups','bit');
    $dbh->do("DELETE FROM fielddefs WHERE name = " . $dbh->quote('groupset'));
}

# September 2002 myk@mozilla.org bug 98801
# Convert the attachment statuses tables into flags tables.
if ($dbh->bz_table_info("attachstatuses") 
    && $dbh->bz_table_info("attachstatusdefs")) 
{
    print "Converting attachment statuses to flags...\n";
    
    # Get IDs for the old attachment status and new flag fields.
    my ($old_field_id) = $dbh->selectrow_array(
        "SELECT fieldid FROM fielddefs WHERE name='attachstatusdefs.name'")
        || 0;
    
    $sth = $dbh->prepare("SELECT fieldid FROM fielddefs " . 
                         "WHERE name='flagtypes.name'");
    $sth->execute();
    my $new_field_id = $sth->fetchrow_arrayref()->[0];

    # Convert attachment status definitions to flag types.  If more than one
    # status has the same name and description, it is merged into a single 
    # status with multiple inclusion records.
    $sth = $dbh->prepare("SELECT id, name, description, sortkey, product_id " . 
                         "FROM attachstatusdefs");
    
    # status definition IDs indexed by name/description
    my $def_ids = {};
    
    # merged IDs and the IDs they were merged into.  The key is the old ID,
    # the value is the new one.  This allows us to give statuses the right
    # ID when we convert them over to flags.  This map includes IDs that
    # weren't merged (in this case the old and new IDs are the same), since 
    # it makes the code simpler.
    my $def_id_map = {};
    
    $sth->execute();
    while (my ($id, $name, $desc, $sortkey, $prod_id) = $sth->fetchrow_array()) {
        my $key = $name . $desc;
        if (!$def_ids->{$key}) {
            $def_ids->{$key} = $id;
            my $quoted_name = $dbh->quote($name);
            my $quoted_desc = $dbh->quote($desc);
            $dbh->do("INSERT INTO flagtypes (id, name, description, sortkey, " .
                     "target_type) VALUES ($id, $quoted_name, $quoted_desc, " .
                     "$sortkey, 'a')");
        }
        $def_id_map->{$id} = $def_ids->{$key};
        $dbh->do("INSERT INTO flaginclusions (type_id, product_id) " . 
                "VALUES ($def_id_map->{$id}, $prod_id)");
    }
    
    # Note: even though we've converted status definitions, we still can't drop
    # the table because we need it to convert the statuses themselves.
    
    # Convert attachment statuses to flags.  To do this we select the statuses
    # from the status table and then, for each one, figure out who set it
    # and when they set it from the bugs activity table.
    my $id = 0;
    $sth = $dbh->prepare(
        "SELECT attachstatuses.attach_id, attachstatusdefs.id, " . 
               "attachstatusdefs.name, attachments.bug_id " . 
          "FROM attachstatuses, attachstatusdefs, attachments " . 
         "WHERE attachstatuses.statusid = attachstatusdefs.id " .
           "AND attachstatuses.attach_id = attachments.attach_id");
    
    # a query to determine when the attachment status was set and who set it
    my $sth2 = $dbh->prepare("SELECT added, who, bug_when " . 
                             "FROM bugs_activity " . 
                             "WHERE bug_id = ? AND attach_id = ? " . 
                             "AND fieldid = $old_field_id " . 
                             "ORDER BY bug_when DESC");
    
    $sth->execute();
    while (my ($attach_id, $def_id, $status, $bug_id) = $sth->fetchrow_array()) {
        ++$id;
        
        # Determine when the attachment status was set and who set it.
        # We should always be able to find out this info from the bug activity,
        # but we fall back to default values just in case.
        $sth2->execute($bug_id, $attach_id);
        my ($added, $who, $when);
        while (($added, $who, $when) = $sth2->fetchrow_array()) {
            last if $added =~ /(^|[, ]+)\Q$status\E([, ]+|$)/;
        }
        $who = $dbh->quote($who); # "NULL" by default if $who is undefined
        $when = $when ? $dbh->quote($when) : "NOW()";
            
        
        $dbh->do("INSERT INTO flags (id, type_id, status, bug_id, attach_id, " .
                 "creation_date, modification_date, requestee_id, setter_id) " . 
                 "VALUES ($id, $def_id_map->{$def_id}, '+', $bug_id, " . 
                 "$attach_id, $when, $when, NULL, $who)");
    }
    
    # Now that we've converted both tables we can drop them.
    $dbh->bz_drop_table("attachstatuses");
    $dbh->bz_drop_table("attachstatusdefs");
    
    # Convert activity records for attachment statuses into records for flags.
    my $sth = $dbh->prepare("SELECT attach_id, who, bug_when, added, removed " .
                            "FROM bugs_activity WHERE fieldid = $old_field_id");
    $sth->execute();
    while (my ($attach_id, $who, $when, $old_added, $old_removed) = 
      $sth->fetchrow_array())
    {
        my @additions = split(/[, ]+/, $old_added);
        @additions = map("$_+", @additions);
        my $new_added = $dbh->quote(join(", ", @additions));
        
        my @removals = split(/[, ]+/, $old_removed);
        @removals = map("$_+", @removals);
        my $new_removed = $dbh->quote(join(", ", @removals));
        
        $old_added = $dbh->quote($old_added);
        $old_removed = $dbh->quote($old_removed);
        $who = $dbh->quote($who);
        $when = $dbh->quote($when);
        
        $dbh->do("UPDATE bugs_activity SET fieldid = $new_field_id, " . 
                 "added = $new_added, removed = $new_removed " . 
                 "WHERE attach_id = $attach_id AND who = $who " . 
                 "AND bug_when = $when AND fieldid = $old_field_id " . 
                 "AND added = $old_added AND removed = $old_removed");
    }
    
    # Remove the attachment status field from the field definitions.
    $dbh->do("DELETE FROM fielddefs WHERE name='attachstatusdefs.name'");

    print "done.\n";
}

# 2004-12-13 Nick.Barnes@pobox.com bug 262268
# Check for spaces and commas in flag type names; if found, rename them.
if ($dbh->bz_table_info("flagtypes")) {
    # Get all names and IDs, to find broken ones and to
    # check for collisions when renaming.
    $sth = $dbh->prepare("SELECT name, id FROM flagtypes");
    $sth->execute();

    my %flagtypes;
    my @badflagnames;
    
    # find broken flagtype names, and populate a hash table
    # to check for collisions.
    while (my ($name, $id) = $sth->fetchrow_array()) {
        $flagtypes{$name} = $id;
        if ($name =~ /[ ,]/) {
            push(@badflagnames, $name);
        }
    }
    if (@badflagnames) {
        print "Removing spaces and commas from flag names...\n";
        my ($flagname, $tryflagname);
        my $sth = $dbh->prepare("UPDATE flagtypes SET name = ? WHERE id = ?");
        foreach $flagname (@badflagnames) {
            print "  Bad flag type name \"$flagname\" ...\n";
            # find a new name for this flagtype.
            ($tryflagname = $flagname) =~ tr/ ,/__/;
            # avoid collisions with existing flagtype names.
            while (defined($flagtypes{$tryflagname})) {
                print "  ... can't rename as \"$tryflagname\" ...\n";
                $tryflagname .= "'";
                if (length($tryflagname) > 50) {
                    my $lastchanceflagname = (substr $tryflagname, 0, 47) . '...';
                    if (defined($flagtypes{$lastchanceflagname})) {
                        print "  ... last attempt as \"$lastchanceflagname\" still failed.'\n",
                              "Rename the flag by hand and run checksetup.pl again.\n";
                        die("Bad flag type name $flagname");
                    }
                    $tryflagname = $lastchanceflagname;
                }
            }
            $sth->execute($tryflagname, $flagtypes{$flagname});
            print "  renamed flag type \"$flagname\" as \"$tryflagname\"\n";
            $flagtypes{$tryflagname} = $flagtypes{$flagname};
            delete $flagtypes{$flagname};
        }
        print "... done.\n";
    }
}

# 2002-11-24 - bugreport@peshkin.net - bug 147275 
#
# If group_control_map is empty, backward-compatbility 
# usebuggroups-equivalent records should be created.
my $entry = Param('useentrygroupdefault');
$sth = $dbh->prepare("SELECT COUNT(*) FROM group_control_map");
$sth->execute();
my ($mapcnt) = $sth->fetchrow_array();
if ($mapcnt == 0) {
    # Initially populate group_control_map.
    # First, get all the existing products and their groups.
    $sth = $dbh->prepare("SELECT groups.id, products.id, groups.name, " .
                         "products.name FROM groups, products " .
                         "WHERE isbuggroup != 0");
    $sth->execute();
    while (my ($groupid, $productid, $groupname, $productname) 
            = $sth->fetchrow_array()) {
        if ($groupname eq $productname) {
            # Product and group have same name.
            $dbh->do("INSERT INTO group_control_map " .
                     "(group_id, product_id, entry, membercontrol, " .
                     "othercontrol, canedit) " .
                     "VALUES ($groupid, $productid, $entry, " .
                     CONTROLMAPDEFAULT . ", " .
                     CONTROLMAPNA . ", 0)");
        } else {
            # See if this group is a product group at all.
            my $sth2 = $dbh->prepare("SELECT id FROM products WHERE name = " .
                                 $dbh->quote($groupname));
            $sth2->execute();
            my ($id) = $sth2->fetchrow_array();
            if (!$id) {
                # If there is no product with the same name as this
                # group, then it is permitted for all products.
                $dbh->do("INSERT INTO group_control_map " .
                         "(group_id, product_id, entry, membercontrol, " .
                         "othercontrol, canedit) " .
                         "VALUES ($groupid, $productid, 0, " .
                         CONTROLMAPSHOWN . ", " .
                         CONTROLMAPNA . ", 0)");
            }
        }
    }
}

# 2004-07-17 GRM - Remove "subscriptions" concept from charting, and add
# group-based security instead. 
if ($dbh->bz_table_info("user_series_map")) {
    # Oracle doesn't like "date" as a column name, and apparently some DBs
    # don't like 'value' either. We use the changes to subscriptions as 
    # something to hang these renamings off.
    $dbh->bz_rename_column('series_data', 'date', 'series_date');
    $dbh->bz_rename_column('series_data', 'value', 'series_value');
    
    # series_categories.category_id produces a too-long column name for the
    # auto-incrementing sequence (Oracle again).
    $dbh->bz_rename_column('series_categories', 'category_id', 'id');
    
    $dbh->bz_add_column("series", "public", 
        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

    # Migrate public-ness across from user_series_map to new field
    $sth = $dbh->prepare("SELECT series_id from user_series_map " .
                         "WHERE user_id = 0");
    $sth->execute();
    while (my ($public_series_id) = $sth->fetchrow_array()) {
        $dbh->do("UPDATE series SET public = 1 " .
                 "WHERE series_id = $public_series_id");
    }

    $dbh->bz_drop_table("user_series_map");
}    

# 2003-06-26 Copy the old charting data into the database, and create the
# queries that will keep it all running. When the old charting system goes
# away, if this code ever runs, it'll just find no files and do nothing.
my $series_exists = $dbh->selectrow_array("SELECT 1 FROM series " .
                                          $dbh->sql_limit(1));
if (!$series_exists) {
    print "Migrating old chart data into database ...\n" unless $silent;
    
    require Bugzilla::Series;
      
    # We prepare the handle to insert the series data    
    my $seriesdatasth = $dbh->prepare("INSERT INTO series_data " . 
                                     "(series_id, series_date, series_value) " .
                                     "VALUES (?, ?, ?)");

    my $deletesth = $dbh->prepare("DELETE FROM series_data 
                                   WHERE series_id = ? AND series_date = ?");
    
    my $groupmapsth = $dbh->prepare("INSERT INTO category_group_map " . 
                                    "(category_id, group_id) " . 
                                    "VALUES (?, ?)");
                                     
    # Fields in the data file (matches the current collectstats.pl)
    my @statuses = 
                qw(NEW ASSIGNED REOPENED UNCONFIRMED RESOLVED VERIFIED CLOSED);
    my @resolutions = 
             qw(FIXED INVALID WONTFIX LATER REMIND DUPLICATE WORKSFORME MOVED);
    my @fields = (@statuses, @resolutions);

    # We have a localisation problem here. Where do we get these values?
    my $all_name = "-All-";
    my $open_name = "All Open";
        
    # We can't give the Series we create a meaningful owner; that's not a big 
    # problem. But we do need to set this global, otherwise Series.pm objects.
    $::userid = 0;
    
    my $products = $dbh->selectall_arrayref("SELECT name FROM products");
     
    foreach my $product ((map { $_->[0] } @$products), "-All-") {
        # First, create the series
        my %queries;
        my %seriesids;
        
        my $query_prod = "";
        if ($product ne "-All-") {
            $query_prod = "product=" . html_quote($product) . "&";
        }
        
        # The query for statuses is different to that for resolutions.
        $queries{$_} = ($query_prod . "bug_status=$_") foreach (@statuses);
        $queries{$_} = ($query_prod . "resolution=$_") foreach (@resolutions);
        
        foreach my $field (@fields) {            
            # Create a Series for each field in this product
            my $series = new Bugzilla::Series(undef, $product, $all_name,
                                              $field, $::userid, 1,
                                              $queries{$field}, 1);
            $series->writeToDatabase();
            $seriesids{$field} = $series->{'series_id'};
        }
        
        # We also add a new query for "Open", so that migrated products get
        # the same set as new products (see editproducts.cgi.)
        my @openedstatuses = ("UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED");
        my $query = join("&", map { "bug_status=$_" } @openedstatuses);
        my $series = new Bugzilla::Series(undef, $product, $all_name,
                                          $open_name, $::userid, 1, 
                                          $query_prod . $query, 1);
        $series->writeToDatabase();
        $seriesids{$open_name} = $series->{'series_id'};
        
        # Now, we attempt to read in historical data, if any
        # Convert the name in the same way that collectstats.pl does
        my $product_file = $product;
        $product_file =~ s/\//-/gs;
        $product_file = "$datadir/mining/$product_file";

        # There are many reasons that this might fail (e.g. no stats for this
        # product), so we don't worry if it does.        
        open(IN, $product_file) or next;

        # The data files should be in a standard format, even for old 
        # Bugzillas, because of the conversion code further up this file.
        my %data;
        my $last_date = "";
        
        while (<IN>) {
            if (/^(\d+\|.*)/) {
                my @numbers = split(/\||\r/, $1);
                
                # Only take the first line for each date; it was possible to
                # run collectstats.pl more than once in a day.
                next if $numbers[0] eq $last_date;
                
                for my $i (0 .. $#fields) {
                    # $numbers[0] is the date
                    $data{$fields[$i]}{$numbers[0]} = $numbers[$i + 1];
                    
                    # Keep a total of the number of open bugs for this day
                    if (IsOpenedState($fields[$i])) {
                        $data{$open_name}{$numbers[0]} += $numbers[$i + 1];
                    }
                }
                
                $last_date = $numbers[0];
            }
        }

        close(IN);

        foreach my $field (@fields, $open_name) {            
            # Insert values into series_data: series_id, date, value
            my %fielddata = %{$data{$field}};
            foreach my $date (keys %fielddata) {
                # We need to delete in case the text file had duplicate entries
                # in it.
                $deletesth->execute($seriesids{$field},
                                    $date);
                         
                # We prepared this above
                $seriesdatasth->execute($seriesids{$field},
                                        $date, 
                                        $fielddata{$date} || 0);
            }
        }
        
        # Create the groupsets for the category
        my $category_id = 
            $dbh->selectrow_array("SELECT id " . 
                                    "FROM series_categories " . 
                                   "WHERE name = " . $dbh->quote($product));
        my $product_id =
            $dbh->selectrow_array("SELECT id " .
                                    "FROM products " . 
                                   "WHERE name = " . $dbh->quote($product));
                                  
        if (defined($category_id) && defined($product_id)) {
          
            # Get all the mandatory groups for this product
            my $group_ids = 
                $dbh->selectcol_arrayref("SELECT group_id " . 
                     "FROM group_control_map " . 
                     "WHERE product_id = $product_id " . 
                     "AND (membercontrol = " . CONTROLMAPMANDATORY . 
                       " OR othercontrol = " . CONTROLMAPMANDATORY . ")");
                                            
            foreach my $group_id (@$group_ids) {
                $groupmapsth->execute($category_id, $group_id);
            }
        }
    }
}

AddFDef("owner_idle_time", "Time Since Assignee Touched", 0);

# 2004-04-12 - Keep regexp-based group permissions up-to-date - Bug 240325
if ($dbh->bz_column_info("user_group_map", "isderived")) {
    $dbh->bz_add_column('user_group_map', 'grant_type', 
        {TYPE => 'INT1', NOTNULL => 1, DEFAULT => '0'});
    $dbh->do("UPDATE user_group_map SET grant_type = " .
                             "IF(isderived, " . GRANT_DERIVED . ", " .
                             GRANT_DIRECT . ")");
    $dbh->do("DELETE FROM user_group_map 
              WHERE isbless = 0 AND grant_type != " . GRANT_DIRECT);
    $dbh->bz_drop_column("user_group_map", "isderived");

    $dbh->bz_drop_index('user_group_map', 'user_group_map_user_id_idx');
    $dbh->bz_add_index('user_group_map', 'user_group_map_user_id_idx',
        {TYPE => 'UNIQUE', 
         FIELDS => [qw(user_id group_id grant_type isbless)]});

    # Evaluate regexp-based group memberships
    my $sth = $dbh->prepare("SELECT profiles.userid, profiles.login_name,
                             groups.id, groups.userregexp 
                             FROM profiles, groups
                             WHERE userregexp != ''");
    $sth->execute();
    my $sth2 = $dbh->prepare("INSERT IGNORE INTO user_group_map 
                           (user_id, group_id, isbless, grant_type) 
                           VALUES(?, ?, 0, " . GRANT_REGEXP . ")");
    while (my ($uid, $login, $gid, $rexp) = $sth->fetchrow_array()) {
        if ($login =~ m/$rexp/i) {
            $sth2->execute($uid, $gid);
        }
    }

    # Make sure groups get rederived
    $dbh->do("UPDATE groups SET last_changed = NOW() WHERE name = 'admin'");
}

# 2004-07-03 - Make it possible to disable flags without deleting them
# from the database. Bug 223878, jouni@heikniemi.net

$dbh->bz_add_column('flags', 'is_active', 
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

# 2004-07-16 - Make it possible to have group-group relationships other than
# membership and bless.
if ($dbh->bz_column_info("group_group_map", "isbless")) {
    $dbh->bz_add_column('group_group_map', 'grant_type', 
        {TYPE => 'INT1', NOTNULL => 1, DEFAULT => '0'});
    $dbh->do("UPDATE group_group_map SET grant_type = " .
                             "IF(isbless, " . GROUP_BLESS . ", " .
                             GROUP_MEMBERSHIP . ")");
    $dbh->bz_drop_index('group_group_map', 'group_group_map_member_id_idx');
    $dbh->bz_drop_column("group_group_map", "isbless");
    $dbh->bz_add_index('group_group_map', 'group_group_map_member_id_idx',
        {TYPE => 'UNIQUE', FIELDS => [qw(member_id grantor_id grant_type)]});
}    

# Allow profiles to optionally be linked to a unique identifier in an outside
# login data source
$dbh->bz_add_column("profiles", "extern_id", {TYPE => 'varchar(64)'});

# 2004-11-20 - LpSolit@netscape.net - Bug 180879
# Add grant and request groups for flags
$dbh->bz_add_column('flagtypes', 'grant_group_id', {TYPE => 'INT3'});
$dbh->bz_add_column('flagtypes', 'request_group_id', {TYPE => 'INT3'});

# 2004-01-03 - bug 253721 erik@dasbistro.com
# mailto is no longer just userids
$dbh->bz_rename_column('whine_schedules', 'mailto_userid', 'mailto');
$dbh->bz_add_column('whine_schedules', 'mailto_type', 
    {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '0'});

# 2005-01-29 - mkanat@bugzilla.org
if (!$dbh->bz_column_info('longdescs', 'already_wrapped')) {
    # Old, pre-wrapped comments should not be auto-wrapped
    $dbh->bz_add_column('longdescs', 'already_wrapped',
        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'}, 1);
    # If an old comment doesn't have a newline in the first 80 characters,
    # (or doesn't contain a newline at all) and it contains a space,
    # then it's probably a mis-wrapped comment and we should wrap it
    # at display-time.
    print "Fixing old, mis-wrapped comments...\n";
    $dbh->do(q{UPDATE longdescs SET already_wrapped = 0
                WHERE (} . $dbh->sql_position(q{'\n'}, 'thetext') . q{ > 80
                   OR } . $dbh->sql_position(q{'\n'}, 'thetext') . q{ = 0)
                  AND SUBSTRING(thetext FROM 1 FOR 80) LIKE '% %'});
}

# 2001-09-03 (landed 2005-02-24)  dkl@redhat.com bug 17453
# Moved enum types to separate tables so we need change the old enum types to 
# standard varchars in the bugs table.
$dbh->bz_alter_column('bugs', 'bug_status', 
                      {TYPE => 'varchar(64)', NOTNULL => 1});
# 2005-03-23 Tomas.Kopal@altap.cz - add default value to resolution, bug 286695
$dbh->bz_alter_column('bugs', 'resolution',
                      {TYPE => 'varchar(64)', NOTNULL => 1, DEFAULT => "''"});
$dbh->bz_alter_column('bugs', 'priority',
                      {TYPE => 'varchar(64)', NOTNULL => 1});
$dbh->bz_alter_column('bugs', 'bug_severity',
                      {TYPE => 'varchar(64)', NOTNULL => 1});
$dbh->bz_alter_column('bugs', 'rep_platform',
                      {TYPE => 'varchar(64)', NOTNULL => 1}, '');
$dbh->bz_alter_column('bugs', 'op_sys',
                      {TYPE => 'varchar(64)', NOTNULL => 1});


# 2005-02-20 - LpSolit@gmail.com - Bug 277504
# When migrating quips from the '$datadir/comments' file to the DB,
# the user ID should be NULL instead of 0 (which is an invalid user ID).
if ($dbh->bz_column_info('quips', 'userid')->{NOTNULL}) {
    $dbh->bz_alter_column('quips', 'userid', {TYPE => 'INT3'});
    print "Changing owner to NULL for quips where the owner is unknown...\n";
    $dbh->do('UPDATE quips SET userid = NULL WHERE userid = 0');
}

# 2005-02-21 - LpSolit@gmail.com - Bug 279910
# qacontact_accessible and assignee_accessible field names no longer exist
# in the 'bugs' table. Their corresponding entries in the 'bugs_activity'
# table should therefore be marked as obsolete, meaning that they cannot
# be used anymore when querying the database - they are not deleted in
# order to keep track of these fields in the activity table.
if (!$dbh->bz_column_info('fielddefs', 'obsolete')) {
    $dbh->bz_add_column('fielddefs', 'obsolete', 
        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    print "Marking qacontact_accessible and assignee_accessible as obsolete fields...\n";
    $dbh->do("UPDATE fielddefs SET obsolete = 1
              WHERE name = 'qacontact_accessible'
                 OR name = 'assignee_accessible'");
}

# Add fulltext indexes for bug summaries and descriptions/comments.
if (!$dbh->bz_index_info('bugs', 'bugs_short_desc_idx')) {
    print "Adding full-text index for short_desc column in bugs table...\n";
    $dbh->bz_add_index('bugs', 'bugs_short_desc_idx', 
                       {TYPE => 'FULLTEXT', FIELDS => [qw(short_desc)]});
}
# Right now, we only create the "thetext" index on MySQL.
if ($dbh->isa('Bugzilla::DB::Mysql') 
    && !$dbh->bz_index_info('longdescs', 'longdescs_thetext_idx')) 
{
    print "Adding full-text index for thetext column in longdescs table...\n";
    $dbh->bz_add_index('longdescs', 'longdescs_thetext_idx',
        {TYPE => 'FULLTEXT', FIELDS => [qw(thetext)]});
}

# 2002 November, myk@mozilla.org, bug 178841:
#
# Convert the "attachments.filename" column from a ridiculously large
# "mediumtext" to a much more sensible "varchar(100)".  Also takes
# the opportunity to remove paths from existing filenames, since they 
# shouldn't be there for security.  Buggy browsers include them, 
# and attachment.cgi now takes them out, but old ones need converting.
#
{
    my $ref = $dbh->bz_column_info("attachments", "filename");
    if ($ref->{TYPE} ne 'varchar(100)') {
        print "Removing paths from filenames in attachments table...\n";
        
        $sth = $dbh->prepare("SELECT attach_id, filename FROM attachments " . 
                "WHERE " . $dbh->sql_position(q{'/'}, 'filename') . " > 0 OR " .
                           $dbh->sql_position(q{'\\\\'}, 'filename') . " > 0");
        $sth->execute;
        
        while (my ($attach_id, $filename) = $sth->fetchrow_array) {
            $filename =~ s/^.*[\/\\]//;
            my $quoted_filename = $dbh->quote($filename);
            $dbh->do("UPDATE attachments SET filename = $quoted_filename " . 
                     "WHERE attach_id = $attach_id");
        }
        
        print "Done.\n";
        
        print "Resizing attachments.filename from mediumtext to varchar(100).\n";
        $dbh->bz_alter_column("attachments", "filename", 
                              {TYPE => 'varchar(100)', NOTNULL => 1});
    }
}

# 2003-01-11, burnus@net-b.de, bug 184309
# Support for quips approval
$dbh->bz_add_column('quips', 'approved', 
    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});
 
# 2002-12-20 Bug 180870 - remove manual shadowdb replication code
$dbh->bz_drop_table("shadowlog");

# 2003-04-27 - bugzilla@chimpychompy.org (GavinS)
#
# Bug 180086 (http://bugzilla.mozilla.org/show_bug.cgi?id=180086)
#
# Renaming the 'count' column in the votes table because Sybase doesn't
# like it
if ($dbh->bz_column_info('votes', 'count')) {
    # 2003-04-24 - myk@mozilla.org/bbaetz@acm.org, bug 201018
    # Force all cached groups to be updated at login, due to security bug
    # Do this here, inside the next schema change block, so that it doesn't
    # get invalidated on every checksetup run.
    $dbh->do("UPDATE profiles SET refreshed_when='1900-01-01 00:00:00'");

    $dbh->bz_rename_column('votes', 'count', 'vote_count');
}

# 2004/02/15 - Summaries shouldn't be null - see bug 220232
$dbh->bz_alter_column('bugs', 'short_desc',
                      {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');

# 2003-10-24 - alt@sonic.net, bug 224208
# Support classification level
$dbh->bz_add_column('products', 'classification_id',
                    {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '1'});

# 2005-01-12 Nick Barnes <nb@ravenbrook.com> bug 278010
# Rename any group which has an empty name.
# Note that there can be at most one such group (because of
# the SQL index on the name column).
$sth = $dbh->prepare("SELECT id FROM groups where name = ''");
$sth->execute();
my ($emptygroupid) = $sth->fetchrow_array();
if ($emptygroupid) {
    # There is a group with an empty name; find a name to rename it
    # as.  Must avoid collisions with existing names.  Start with
    # group_$gid and add _<n> if necessary.
    my $trycount = 0;
    my $trygroupname;
    my $trygroupsth = $dbh->prepare("SELECT id FROM groups where name = ?");
    do {
        $trygroupname = "group_$emptygroupid";
        if ($trycount > 0) {
            $trygroupname .= "_$trycount";
        }
        $trygroupsth->execute($trygroupname);
        if ($trygroupsth->rows > 0) {
            $trycount ++;
        }
    } while ($trygroupsth->rows > 0);
    $sth = $dbh->prepare("UPDATE groups SET name = ? " .
                         "WHERE id = $emptygroupid");
    $sth->execute($trygroupname);
    print "Group $emptygroupid had an empty name; renamed as '$trygroupname'.\n";
}

# 2005-02-12 bugreport@peshkin.net, bug 281787
$dbh->bz_add_index('bugs_activity', 'bugs_activity_who_idx', [qw(who)]);

# This lastdiffed change and these default changes are unrelated,
# but in order for MySQL to successfully run these default changes only once,
# they have to be inside this block.
# If bugs.lastdiffed is NOT NULL...
if($dbh->bz_column_info('bugs', 'lastdiffed')->{NOTNULL}) {
    # Add defaults for some fields that should have them but didn't.
    $dbh->bz_alter_column('bugs', 'status_whiteboard',
        {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});
    $dbh->bz_alter_column('bugs', 'keywords',
        {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});
    $dbh->bz_alter_column('bugs', 'votes',
                          {TYPE => 'INT3', NOTNULL => 1, DEFAULT => '0'});
    # And change lastdiffed to NULL
    $dbh->bz_alter_column('bugs', 'lastdiffed', {TYPE => 'DATETIME'});
}

# 2005-03-09 qa_contact should be NULL instead of 0, bug 285534
if ($dbh->bz_column_info('bugs', 'qa_contact')->{NOTNULL}) {
    $dbh->bz_alter_column('bugs', 'qa_contact', {TYPE => 'INT3'});
    $dbh->do("UPDATE bugs SET qa_contact = NULL WHERE qa_contact = 0");
}

# 2005-03-27 initialqacontact should be NULL instead of 0, bug 287483
if ($dbh->bz_column_info('components', 'initialqacontact')->{NOTNULL}) {
    $dbh->bz_alter_column('components', 'initialqacontact', {TYPE => 'INT3'});
    $dbh->do("UPDATE components SET initialqacontact = NULL " .
             "WHERE initialqacontact = 0");
}

# 2005-03-29 - gerv@gerv.net - bug 73665.
# Migrate email preferences to new email prefs table.
if ($dbh->bz_column_info("profiles", "emailflags")) {
    print "Migrating email preferences to new table ...\n" unless $silent;
    
    # These are the "roles" and "reasons" from the original code, mapped to
    # the new terminology of relationships and events.
    my %relationships = ("Owner" => REL_ASSIGNEE, 
                         "Reporter" => REL_REPORTER,
                         "QAcontact" => REL_QA,
                         "CClist" => REL_CC,
                         "Voter" => REL_VOTER);
                         
    my %events = ("Removeme" => EVT_ADDED_REMOVED, 
                  "Comments" => EVT_COMMENT, 
                  "Attachments" => EVT_ATTACHMENT, 
                  "Status" => EVT_PROJ_MANAGEMENT, 
                  "Resolved" => EVT_OPENED_CLOSED,
                  "Keywords" => EVT_KEYWORD, 
                  "CC" => EVT_CC, 
                  "Other" => EVT_OTHER,
                  "Unconfirmed" => EVT_UNCONFIRMED);
                                    
    # Request preferences
    my %requestprefs = ("FlagRequestee" => EVT_FLAG_REQUESTED,
                        "FlagRequester" => EVT_REQUESTED_FLAG);

    # Select all emailflags flag strings
    my $sth = $dbh->prepare("SELECT userid, emailflags FROM profiles");
    $sth->execute();

    while (my ($userid, $flagstring) = $sth->fetchrow_array()) {
        # If the user has never logged in since emailprefs arrived, and the
        # temporary code to give them a default string never ran, then 
        # $flagstring will be null. In this case, they just get all mail.
        $flagstring ||= "";
        
        # The 255 param is here, because without a third param, split will
        # trim any trailing null fields, which causes Perl to eject lots of
        # warnings. Any suitably large number would do.
        my %emailflags = split(/~/, $flagstring, 255); # Appease my editor: /
     
        my $sth2 = $dbh->prepare("INSERT into email_setting " .
                                 "(user_id, relationship, event) VALUES (" . 
                                 "$userid, ?, ?)");
        foreach my $relationship (keys %relationships) {
            foreach my $event (keys %events) {
                my $key = "email$relationship$event";
                if (!exists($emailflags{$key}) || $emailflags{$key} eq 'on') {
                    $sth2->execute($relationships{$relationship},
                                   $events{$event});
                }
            }
        }

        # Note that in the old system, the value of "excludeself" is assumed to
        # be off if the preference does not exist in the user's list, unlike 
        # other preferences whose value is assumed to be on if they do not 
        # exist.
        #
        # This preference has changed from global to per-relationship.
        if (!exists($emailflags{'ExcludeSelf'}) 
            || $emailflags{'ExcludeSelf'} ne 'on')
        {
            foreach my $relationship (keys %relationships) {
                $dbh->do("INSERT into email_setting " .
                         "(user_id, relationship, event) VALUES (" . 
                         $userid . ", " .
                         $relationships{$relationship}. ", " .
                         EVT_CHANGED_BY_ME . ")");
            }
        }

        foreach my $key (keys %requestprefs) {
            if (!exists($emailflags{$key}) || $emailflags{$key} eq 'on') {
              $dbh->do("INSERT into email_setting " . 
                       "(user_id, relationship, event) VALUES (" . 
                       $userid . ", " . REL_ANY . ", " . 
                       $requestprefs{$key} . ")");            
            }
        }
    }
   
    # EVT_ATTACHMENT_DATA should initially have identical settings to 
    # EVT_ATTACHMENT. 
    CloneEmailEvent(EVT_ATTACHMENT, EVT_ATTACHMENT_DATA); 
       
    $dbh->bz_drop_column("profiles", "emailflags");    
}

sub CloneEmailEvent {
    my ($source, $target) = @_;

    my $sth1 = $dbh->prepare("SELECT user_id, relationship FROM email_setting
                              WHERE event = $source");
    my $sth2 = $dbh->prepare("INSERT into email_setting " . 
                             "(user_id, relationship, event) VALUES (" . 
                             "?, ?, $target)");
    
    $sth1->execute();

    while (my ($userid, $relationship) = $sth1->fetchrow_array()) {
        $sth2->execute($userid, $relationship);            
    }    
} 

# 2005-03-27: Standardize all boolean fields to plain "tinyint"
if ( $dbh->isa('Bugzilla::DB::Mysql') ) {
    # This is a change to make things consistent with Schema, so we use
    # direct-database access methods.
    my $quip_info_sth = $dbh->column_info(undef, undef, 'quips', '%');
    my $quips_cols    = $quip_info_sth->fetchall_hashref("COLUMN_NAME");
    my $approved_col  = $quips_cols->{'approved'};
    if ( $approved_col->{TYPE_NAME} eq 'TINYINT'
         and $approved_col->{COLUMN_SIZE} == 1 )
    {
        $dbh->bz_alter_column_raw('series', 'public',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '0'});
        $dbh->bz_alter_column_raw('bug_status', 'isactive',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
        $dbh->bz_alter_column_raw('rep_platform', 'isactive',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
        $dbh->bz_alter_column_raw('resolution', 'isactive',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
        $dbh->bz_alter_column_raw('op_sys', 'isactive',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
        $dbh->bz_alter_column_raw('bug_severity', 'isactive',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
        $dbh->bz_alter_column_raw('priority', 'isactive',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
        $dbh->bz_alter_column_raw('quips', 'approved',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
    }
}

# 2005-04-07 - alt@sonic.net, bug 289455
# make classification_id field type be consistent with DB:Schema
$dbh->bz_alter_column('products', 'classification_id',
                      {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '1'});

# initialowner was accidentally NULL when we checked-in Schema,
# when it really should be NOT NULL.
$dbh->bz_alter_column('components', 'initialowner',
                      {TYPE => 'INT3', NOTNULL => 1}, 0);

# 2005-03-28 - bug 238800 - index flags.type_id to make editflagtypes.cgi speedy
$dbh->bz_add_index('flags', 'flags_type_id_idx', [qw(type_id)]);

# For a short time, the flags_type_id_idx was misnamed in upgraded installs.
$dbh->bz_drop_index('flags', 'type_id');

# 2005-04-28 - LpSolit@gmail.com - Bug 7233: add an index to versions
$dbh->bz_alter_column('versions', 'value',
                      {TYPE => 'varchar(64)', NOTNULL => 1});
$dbh->bz_add_index('versions', 'versions_product_id_idx',
                   {TYPE => 'UNIQUE', FIELDS => [qw(product_id value)]});

# Milestone sortkeys get a default just like all other sortkeys.
if (!exists $dbh->bz_column_info('milestones', 'sortkey')->{DEFAULT}) {
    $dbh->bz_alter_column('milestones', 'sortkey', 
                          {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});
}

# 2005-06-14 - LpSolit@gmail.com - Bug 292544: only set creation_ts
# when all bug fields have been correctly set.
$dbh->bz_alter_column('bugs', 'creation_ts', {TYPE => 'DATETIME'});

if (!exists $dbh->bz_column_info('whine_queries', 'title')->{DEFAULT}) {

    # The below change actually has nothing to do with the whine_queries
    # change, it just has to be contained within a schema change so that
    # it doesn't run every time we run checksetup.

    # Old Bugzillas have "other" as an OS choice, new ones have "Other"
    # (capital O).
    # XXX - This should be moved inside of a later schema change, once
    #       we have one to move it to the inside of.
    print "Setting any 'other' op_sys to 'Other'...\n";
    $dbh->do('UPDATE op_sys SET value = ? WHERE value = ?',
             undef, "Other", "other");
    $dbh->do('UPDATE bugs SET op_sys = ? WHERE op_sys = ?',
             undef, "Other", "other");
    if (Param('defaultopsys') eq 'other') {
        # We can't actually fix the param here, because WriteParams() will
        # make $datadir/params unwriteable to the webservergroup.
        # It's too much of an ugly hack to copy the permission-fixing code
        # down to here. (It would create more potential future bugs than
        # it would solve problems.)
        print "WARNING: Your 'defaultopsys' param is set to 'other', but"
            . " Bugzilla now\n"
            . "         uses 'Other' (capital O).\n";
    }

    # Add a DEFAULT to whine_queries stuff so that editwhines.cgi
    # works on PostgreSQL.
    $dbh->bz_alter_column('whine_queries', 'title', {TYPE => 'varchar(128)', 
                          NOTNULL => 1, DEFAULT => "''"});
}

# 2005-12-30 - wurblzap@gmail.com - Bug 300473
# Repair broken automatically generated series queries for non-open bugs.
my $broken_series_indicator =
    'field0-0-0=resolution&type0-0-0=notequals&value0-0-0=---';
my $broken_nonopen_series =
    $dbh->selectall_arrayref("SELECT series_id, query FROM series
                               WHERE query LIKE '$broken_series_indicator%'");
if (@$broken_nonopen_series) {
    print 'Repairing broken series...';
    my $sth_nuke =
        $dbh->prepare('DELETE FROM series_data WHERE series_id = ?');
    # This statement is used to repair a series by replacing the broken query
    # with the correct one.
    my $sth_repair =
        $dbh->prepare('UPDATE series SET query = ? WHERE series_id = ?');
    # The corresponding series for open bugs look like one of these two
    # variations (bug 225687 changed the order of bug states).
    # This depends on the set of bug states representing open bugs not to have
    # changed since series creation.
    my $open_bugs_query_base_old = 
        join("&", map { "bug_status=" . url_quote($_) }
                      ('UNCONFIRMED', 'NEW', 'ASSIGNED', 'REOPENED'));
    my $open_bugs_query_base_new = 
        join("&", map { "bug_status=" . url_quote($_) } OpenStates());
    my $sth_openbugs_series =
        $dbh->prepare("SELECT series_id FROM series
                        WHERE query IN (?, ?)");
    # Statement to find the series which has collected the most data.
    my $sth_data_collected =
        $dbh->prepare('SELECT count(*) FROM series_data WHERE series_id = ?');
    # Statement to select a broken non-open bugs count data entry.
    my $sth_select_broken_nonopen_data =
        $dbh->prepare('SELECT series_date, series_value FROM series_data' .
                      ' WHERE series_id = ?');
    # Statement to select an open bugs count data entry.
    my $sth_select_open_data =
        $dbh->prepare('SELECT series_value FROM series_data' .
                      ' WHERE series_id = ? AND series_date = ?');
    # Statement to fix a broken non-open bugs count data entry.
    my $sth_fix_broken_nonopen_data =
        $dbh->prepare('UPDATE series_data SET series_value = ?' .
                      ' WHERE series_id = ? AND series_date = ?');
    # Statement to delete an unfixable broken non-open bugs count data entry.
    my $sth_delete_broken_nonopen_data =
        $dbh->prepare('DELETE FROM series_data' .
                      ' WHERE series_id = ? AND series_date = ?');

    foreach (@$broken_nonopen_series) {
        my ($broken_series_id, $nonopen_bugs_query) = @$_;

        # Determine the product-and-component part of the query.
        if ($nonopen_bugs_query =~ /^$broken_series_indicator(.*)$/) {
            my $prodcomp = $1;

            # If there is more than one series for the corresponding open-bugs
            # series, we pick the one with the most data, which should be the
            # one which was generated on creation.
            # It's a pity we can't do subselects.
            $sth_openbugs_series->execute($open_bugs_query_base_old . $prodcomp,
                                          $open_bugs_query_base_new . $prodcomp);
            my ($found_open_series_id, $datacount) = (undef, -1);
            foreach my $open_series_id ($sth_openbugs_series->fetchrow_array()) {
                $sth_data_collected->execute($open_series_id);
                my ($this_datacount) = $sth_data_collected->fetchrow_array();
                if ($this_datacount > $datacount) {
                    $datacount = $this_datacount;
                    $found_open_series_id = $open_series_id;
                }
            }

            if ($found_open_series_id) {
                # Move along corrupted series data and correct it. The
                # corruption consists of it being the number of all bugs
                # instead of the number of non-open bugs, so we calculate the
                # correct count by subtracting the number of open bugs.
                # If there is no corresponding open-bugs count for some reason
                # (shouldn't happen), we drop the data entry.
                print " $broken_series_id...";
                $sth_select_broken_nonopen_data->execute($broken_series_id);
                while (my $rowref =
                       $sth_select_broken_nonopen_data->fetchrow_arrayref()) {
                    my ($date, $broken_value) = @$rowref;
                    my ($openbugs_value) =
                        $dbh->selectrow_array($sth_select_open_data, undef,
                                              $found_open_series_id, $date);
                    if (defined($openbugs_value)) {
                        $sth_fix_broken_nonopen_data->execute
                            ($broken_value - $openbugs_value,
                             $broken_series_id, $date);
                    }
                    else {
                        print "\nWARNING - During repairs of series " .
                              "$broken_series_id, the irreparable data\n" .
                              "entry for date $date was encountered and is " .
                              "being deleted.\n" .
                              "Continuing repairs...";
                        $sth_delete_broken_nonopen_data->execute
                            ($broken_series_id, $date);
                    }
                }

                # Fix the broken query so that it collects correct data in the
                # future.
                $nonopen_bugs_query =~
                    s/^$broken_series_indicator/field0-0-0=resolution&type0-0-0=regexp&value0-0-0=./;
                $sth_repair->execute($nonopen_bugs_query, $broken_series_id);
            }
            else {
                print "\nWARNING - Series $broken_series_id was meant to\n" .
                      "collect non-open bug counts, but it has counted\n" .
                      "all bugs instead. It cannot be repaired\n" .
                      "automatically because no series that collected open\n" .
                      "bug counts was found. You'll probably want to delete\n" .
                      "or repair collected data for series $broken_series_id " .
                      "manually.\n" .
                      "Continuing repairs...";
            }
        }
    }
    print " done.\n";
}

# Fixup for Bug 101380
# "Newlines, nulls, leading/trailing spaces are getting into summaries"

my $controlchar_bugs =
    $dbh->selectall_arrayref("SELECT short_desc, bug_id FROM bugs WHERE " .
                             "'short_desc' " . $dbh->sql_regexp . 
                             " '[[:cntrl:]]'");
if (scalar(@$controlchar_bugs))
{
    my $msg = 'Cleaning control characters from bug summaries...';
    my $found = 0;
    foreach (@$controlchar_bugs) {
        my ($short_desc, $bug_id) = @$_;
        my $clean_short_desc = clean_text($short_desc);
        if ($clean_short_desc ne $short_desc) {
            print $msg if !$found;
            $found = 1;
            print " $bug_id...";
            $dbh->do("UPDATE bugs SET short_desc = ? WHERE bug_id = ?",
                      undef, $clean_short_desc, $bug_id);
        }
    }
    print " done.\n" if $found;
}

# If you had to change the --TABLE-- definition in any way, then add your
# differential change code *** A B O V E *** this comment.
#
# That is: if you add a new field, you first search for the first occurrence
# of --TABLE-- and add your field to into the table hash. This new setting
# would be honored for every new installation. Then add your
# bz_add_field/bz_drop_field/bz_change_field_type/bz_rename_field code above.
# This would then be honored by everyone who updates his Bugzilla installation.
#

#
# BugZilla uses --GROUPS-- to assign various rights to its users.
#

AddGroup('tweakparams', 'Can tweak operating parameters');
AddGroup('editusers', 'Can edit or disable users');
AddGroup('creategroups', 'Can create and destroy groups.');
AddGroup('editclassifications', 'Can create, destroy, and edit classifications.');
AddGroup('editcomponents', 'Can create, destroy, and edit components.');
AddGroup('editkeywords', 'Can create, destroy, and edit keywords.');
AddGroup('admin', 'Administrators');

# 2005-06-29 bugreport@peshkin.net, bug 299156
if ($dbh->bz_index_info('attachments', 'attachments_submitter_id_idx') 
   && (scalar(@{$dbh->bz_index_info('attachments', 
                                    'attachments_submitter_id_idx'
                                   )->{FIELDS}}) < 2)
      ) {
    $dbh->bz_drop_index('attachments', 'attachments_submitter_id_idx');
}
$dbh->bz_add_index('attachments', 'attachments_submitter_id_idx',
                   [qw(submitter_id bug_id)]);

if (!GroupDoesExist("editbugs")) {
    my $id = AddGroup('editbugs', 'Can edit all bug fields.', ".*");
    my $sth = $dbh->prepare("SELECT userid FROM profiles");
    $sth->execute();
    while (my ($userid) = $sth->fetchrow_array()) {
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $id, 0, " . GRANT_DIRECT . ")");
    }
}

if (!GroupDoesExist("canconfirm")) {
    my $id = AddGroup('canconfirm',  'Can confirm a bug.', ".*");
    my $sth = $dbh->prepare("SELECT userid FROM profiles");
    $sth->execute();
    while (my ($userid) = $sth->fetchrow_array()) {
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $id, 0, " . GRANT_DIRECT . ")");
    }

}

# Create bz_canusewhineatothers and bz_canusewhines
if (!GroupDoesExist('bz_canusewhines')) {
    my $whine_group = AddGroup('bz_canusewhines',
                               'User can configure whine reports for self');
    my $whineatothers_group = AddGroup('bz_canusewhineatothers',
                                       'Can configure whine reports for ' .
                                       'other users');
    my $group_exists = $dbh->selectrow_array(
        q{SELECT 1 FROM group_group_map 
           WHERE member_id = ? AND grantor_id = ? AND grant_type = ?},
        undef, $whineatothers_group, $whine_group, GROUP_MEMBERSHIP);
    $dbh->do("INSERT INTO group_group_map " .
             "(member_id, grantor_id, grant_type) " .
             "VALUES (${whineatothers_group}, ${whine_group}, " .
             GROUP_MEMBERSHIP . ")") unless $group_exists;
}

###########################################################################
# Create --SETTINGS-- users can adjust
###########################################################################

# 2005-03-03 travis@sedsystems.ca -- Bug 41972
add_setting ("display_quips", {"on" => 1, "off" => 2 }, "on" );

# 2005-03-10 travis@sedsystems.ca -- Bug 199048
add_setting ("comment_sort_order", {"oldest_to_newest" => 1,
                                    "newest_to_oldest" => 2,
                                    "newest_to_oldest_desc_first" => 3}, 
             "oldest_to_newest" );

# 2005-06-29 wurblzap@gmail.com -- Bug 257767
add_setting ('csv_colsepchar', {',' => 1, ';' => 2 }, ',' );

###########################################################################
# Create Administrator  --ADMIN--
###########################################################################


sub bailout {   # this is just in case we get interrupted while getting passwd
    if ($^O !~ /MSWin32/i) {
        system("stty","echo"); # re-enable input echoing
    }
    exit 1;
}

if (@admins) {
    # Identify admin group.
    my $sth = $dbh->prepare("SELECT id FROM groups 
                WHERE name = 'admin'");
    $sth->execute();
    my ($adminid) = $sth->fetchrow_array();
    foreach my $userid (@admins) {
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $adminid, 0, " . GRANT_DIRECT . ")");
        # Existing administrators are made blessers of group "admin"
        # but only explicitly defined blessers can bless group admin.
        # Other groups can be blessed by any admin (by default) or additional
        # defined blessers.
        $dbh->do("INSERT INTO user_group_map 
            (user_id, group_id, isbless, grant_type) 
            VALUES ($userid, $adminid, 1, " . GRANT_DIRECT . ")");
    }
    $sth = $dbh->prepare("SELECT id FROM groups");
    $sth->execute();
    while ( my ($id) = $sth->fetchrow_array() ) {
        # Admins can bless every group.
        $dbh->do("INSERT INTO group_group_map 
            (member_id, grantor_id, grant_type) 
            VALUES ($adminid, $id," . GROUP_BLESS . ")");
        # Admins can see every group.
        $dbh->do("INSERT INTO group_group_map 
            (member_id, grantor_id, grant_type) 
            VALUES ($adminid, $id," . GROUP_VISIBLE . ")");
        # Admins are initially members of every group.
        next if ($id == $adminid);
        $dbh->do("INSERT INTO group_group_map 
            (member_id, grantor_id, grant_type) 
            VALUES ($adminid, $id," . GROUP_MEMBERSHIP . ")");
    }
}


my @groups = ();
$sth = $dbh->prepare("SELECT id FROM groups");
$sth->execute();
while ( my @row = $sth->fetchrow_array() ) {
    push (@groups, $row[0]);
}

#  Prompt the user for the email address and name of an administrator.  Create
#  that login, if it doesn't exist already, and make it a member of all groups.

$sth = $dbh->prepare("SELECT user_id FROM groups INNER JOIN user_group_map " .
                     "ON id = group_id WHERE name = 'admin'");
$sth->execute;
# when we have no admin users, prompt for admin email address and password ...
if ($sth->rows == 0) {
    my $login = "";
    my $realname = "";
    my $pass1 = "";
    my $pass2 = "*";
    my $admin_ok = 0;
    my $admin_create = 1;
    my $mailcheckexp = "";
    my $mailcheck    = ""; 

    # Here we look to see what the emailregexp is set to so we can 
    # check the email addy they enter. Bug 96675. If they have no 
    # params (likely but not always the case), we use the default.
    if (-e "$datadir/params") { 
        require "$datadir/params"; # if they have a params file, use that
    }
    if (Param('emailregexp')) {
        $mailcheckexp = Param('emailregexp');
        $mailcheck    = Param('emailregexpdesc');
    } else {
        $mailcheckexp = '^[\\w\\.\\+\\-=]+@[\\w\\.\\-]+\\.[\\w\\-]+$';
        $mailcheck    = 'A legal address must contain exactly one \'@\', 
        and at least one \'.\' after the @.';
    }

    print "\nLooks like we don't have an administrator set up yet.\n";
    print "Either this is your first time using Bugzilla, or your\n ";
    print "administrator's privileges might have accidentally been deleted.\n";
    while(! $admin_ok ) {
        while( $login eq "" ) {
            print "Enter the e-mail address of the administrator: ";
            $login = $answer{'ADMIN_EMAIL'} 
                || ($silent && die("cant preload ADMIN_EMAIL")) 
                || <STDIN>;
            chomp $login;
            if(! $login ) {
                print "\nYou DO want an administrator, don't you?\n";
            }
            unless ($login =~ /$mailcheckexp/) {
                print "\nThe login address is invalid:\n";
                print "$mailcheck\n";
                print "You can change this test on the params page once checksetup has successfully\n";
                print "completed.\n\n";
                # Go round, and ask them again
                $login = "";
            }
        }
        $sth = $dbh->prepare("SELECT login_name FROM profiles " .
                              "WHERE " . $dbh->sql_istrcmp('login_name', '?'));
        $sth->execute($login);
        if ($sth->rows > 0) {
            print "$login already has an account.\n";
            print "Make this user the administrator? [Y/n] ";
            my $ok = $answer{'ADMIN_OK'} 
                || ($silent && die("cant preload ADMIN_OK")) 
                || <STDIN>;
            chomp $ok;
            if ($ok !~ /^n/i) {
                $admin_ok = 1;
                $admin_create = 0;
            } else {
                print "OK, well, someone has to be the administrator.\n";
                print "Try someone else.\n";
                $login = "";
            }
        } else {
            print "You entered $login.  Is this correct? [Y/n] ";
            my $ok = $answer{'ADMIN_OK'} 
                || ($silent && die("cant preload ADMIN_OK")) 
                || <STDIN>;
            chomp $ok;
            if ($ok !~ /^n/i) {
                $admin_ok = 1;
            } else {
                print "That's okay, typos happen.  Give it another shot.\n";
                $login = "";
            }
        }
    }

    if ($admin_create) {

        while( $realname eq "" ) {
            print "Enter the real name of the administrator: ";
            $realname = $answer{'ADMIN_REALNAME'} 
                || ($silent && die("cant preload ADMIN_REALNAME")) 
                || <STDIN>;
            chomp $realname;
            if(! $realname ) {
                print "\nReally.  We need a full name.\n";
            }
        }

        # trap a few interrupts so we can fix the echo if we get aborted.
        $SIG{HUP}  = \&bailout;
        $SIG{INT}  = \&bailout;
        $SIG{QUIT} = \&bailout;
        $SIG{TERM} = \&bailout;

        if ($^O !~ /MSWin32/i) {
            system("stty","-echo");  # disable input echoing
        }

        while( $pass1 ne $pass2 ) {
            while( $pass1 eq "" || $pass1 !~ /^[[:print:]]{3,16}$/ ) {
                print "Enter a password for the administrator account: ";
                $pass1 = $answer{'ADMIN_PASSWORD'} 
                    || ($silent && die("cant preload ADMIN_PASSWORD")) 
                    || <STDIN>;
                chomp $pass1;
                if(! $pass1 ) {
                    print "\n\nAn empty password is a security risk. Try again!\n";
                } elsif ( $pass1 !~ /^.{3,16}$/ ) {
                    print "\n\nThe password must be 3-16 characters in length.\n";
                } elsif ( $pass1 !~ /^[[:print:]]{3,16}$/ ) {
                    print "\n\nThe password contains non-printable characters.\n";
                }
            }
            print "\nPlease retype the password to verify: ";
            $pass2 = $answer{'ADMIN_PASSWORD'} 
                || ($silent && die("cant preload ADMIN_PASSWORD")) 
                || <STDIN>;
            chomp $pass2;
            if ($pass1 ne $pass2) {
                print "\n\nPasswords don't match.  Try again!\n";
                $pass1 = "";
                $pass2 = "*";
            }
        }

        if ($^O !~ /MSWin32/i) {
            system("stty","echo"); # re-enable input echoing
        }

        $SIG{HUP}  = 'DEFAULT'; # and remove our interrupt hooks
        $SIG{INT}  = 'DEFAULT';
        $SIG{QUIT} = 'DEFAULT';
        $SIG{TERM} = 'DEFAULT';

        insert_new_user($login, $realname, $pass1);
    }

    # Put the admin in each group if not already    
    my $userid = $dbh->selectrow_array("SELECT userid FROM profiles WHERE " .
                                       $dbh->sql_istrcmp('login_name', '?'),
                                       undef, $login);

    # Admins get explicit membership and bless capability for the admin group
    my ($admingroupid) = $dbh->selectrow_array("SELECT id FROM groups 
                                                WHERE name = 'admin'");
    $dbh->do("INSERT INTO user_group_map 
        (user_id, group_id, isbless, grant_type) 
        VALUES ($userid, $admingroupid, 0, " . GRANT_DIRECT . ")");
    $dbh->do("INSERT INTO user_group_map 
        (user_id, group_id, isbless, grant_type) 
        VALUES ($userid, $admingroupid, 1, " . GRANT_DIRECT . ")");

    # Admins get inherited membership and bless capability for all groups
    foreach my $group ( @groups ) {
        my $sth_check = $dbh->prepare("SELECT member_id FROM group_group_map
                                 WHERE member_id = ?
                                 AND  grantor_id = ?
                                 AND grant_type = ?");
        $sth_check->execute($admingroupid, $group, GROUP_MEMBERSHIP);
        unless ($sth_check->rows) {
            $dbh->do("INSERT INTO group_group_map
                      (member_id, grantor_id, grant_type)
                      VALUES ($admingroupid, $group, " . GROUP_MEMBERSHIP . ")");
        }
        $sth_check->execute($admingroupid, $group, GROUP_BLESS);
        unless ($sth_check->rows) {
            $dbh->do("INSERT INTO group_group_map
                      (member_id, grantor_id, grant_type)
                      VALUES ($admingroupid, $group, " . GROUP_BLESS . ")");
        }
    }

    print "\n$login is now set up as an administrator account.\n";
}


#
# Final checks...

$sth = $dbh->prepare("SELECT user_id " .
                       "FROM groups INNER JOIN user_group_map " .
                       "ON groups.id = user_group_map.group_id " .
                      "WHERE groups.name = 'admin'");
$sth->execute;
my ($adminuid) = $sth->fetchrow_array;
if (!$adminuid) { die "No administrator!" } # should never get here
# when test product was created, admin was unknown
$dbh->do("UPDATE components " .
            "SET initialowner = $adminuid " .
          "WHERE initialowner = 0");

unlink "$datadir/versioncache";

################################################################################
