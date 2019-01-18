# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Install::Filesystem;

# NOTE: This package may "use" any modules that it likes,
# and localconfig is available. However, all functions in this
# package should assume that:
#
# * Templates are not available.
# * Files do not have the correct permissions.
# * The database does not exist.

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Install::Localconfig;
use Bugzilla::Install::Util qw(install_string);
use Bugzilla::Util;
use Bugzilla::Hook;

use File::Find;
use File::Path;
use File::Basename;
use File::Copy qw(move);
use File::Spec;
use IO::File;
use POSIX ();

use parent qw(Exporter);
our @EXPORT = qw(
    update_filesystem
    create_htaccess
    fix_all_file_permissions
    fix_dir_permissions
    fix_file_permissions
);

use constant HT_DEFAULT_DENY => <<EOT;
# nothing in this directory is retrievable unless overridden by an .htaccess
# in a subdirectory
<IfModule mod_version.c>
  <IfVersion < 2.4>
    Deny from all
  </IfVersion>
  <IfVersion >= 2.4>
    Require all denied
  </IfVersion>
</IfModule>
<IfModule !mod_version.c>
  Deny from all
</IfModule>
EOT

###############
# Permissions #
###############

# Used by the permissions "constants" below.
sub _suexec { Bugzilla->localconfig->{'use_suexec'}     };
sub _group  { Bugzilla->localconfig->{'webservergroup'} };

# Writeable by the owner only.
use constant OWNER_WRITE => 0600;
# Executable by the owner only.
use constant OWNER_EXECUTE => 0700;
# A directory which is only writeable by the owner.
use constant DIR_OWNER_WRITE => 0700;

# A cgi script that the webserver can execute.
sub WS_EXECUTE { _group() ? 0750 : 0755 };
# A file that is read by cgi scripts, but is not ever read
# directly by the webserver.
sub CGI_READ { _group() ? 0640 : 0644 };
# A file that is written to by cgi scripts, but is not ever
# read or written directly by the webserver.
sub CGI_WRITE { _group() ? 0660 : 0666 };
# A file that is served directly by the web server.
sub WS_SERVE { (_group() and !_suexec()) ? 0640 : 0644 };

# A directory whose contents can be read or served by the
# webserver (so even directories containing cgi scripts
# would have this permission).
sub DIR_WS_SERVE { (_group() and !_suexec()) ? 0750 : 0755 };
# A directory that is read by cgi scripts, but is never accessed
# directly by the webserver
sub DIR_CGI_READ { _group() ? 0750 : 0755 };
# A directory that is written to by cgi scripts, but where the
# scripts never needs to overwrite files created by other
# users.
sub DIR_CGI_WRITE { _group() ? 0770 : 01777 };
# A directory that is written to by cgi scripts, where the
# scripts need to overwrite files created by other users.
sub DIR_CGI_OVERWRITE { _group() ? 0770 : 0777 };

# This can be combined (using "|") with other permissions for 
# directories that, in addition to their normal permissions (such
# as DIR_CGI_WRITE) also have content served directly from them
# (or their subdirectories) to the user, via the webserver.
sub DIR_ALSO_WS_SERVE { _suexec() ? 0001 : 0 };

# This looks like a constant because it effectively is, but
# it has to call other subroutines and read the current filesystem,
# so it's defined as a sub. This is not exported, so it doesn't have
# a perldoc. However, look at the various hashes defined inside this 
# function to understand what it returns. (There are comments throughout.)
#
# The rationale for the file permissions is that there is a group the
# web server executes the scripts as, so the cgi scripts should not be writable
# by this group. Otherwise someone may find it possible to change the cgis
# when exploiting some security flaw somewhere (not necessarily in Bugzilla!)
sub FILESYSTEM {
    my $datadir       = bz_locations()->{'datadir'};
    my $attachdir     = bz_locations()->{'attachdir'};
    my $extensionsdir = bz_locations()->{'extensionsdir'};
    my $webdotdir     = bz_locations()->{'webdotdir'};
    my $templatedir   = bz_locations()->{'templatedir'};
    my $libdir        = bz_locations()->{'libpath'};
    my $extlib        = bz_locations()->{'ext_libpath'};
    my $skinsdir      = bz_locations()->{'skinsdir'};
    my $localconfig   = bz_locations()->{'localconfig'};
    my $template_cache = bz_locations()->{'template_cache'};
    my $graphsdir     = bz_locations()->{'graphsdir'};
    my $assetsdir     = bz_locations()->{'assetsdir'};

    # We want to set the permissions the same for all localconfig files
    # across all PROJECTs, so we do something special with $localconfig,
    # lower down in the permissions section.
    if ($ENV{PROJECT}) {
        $localconfig =~ s/\.\Q$ENV{PROJECT}\E$//;
    }

    # Note: When being processed by checksetup, these have their permissions
    # set in this order: %all_dirs, %recurse_dirs, %all_files.
    #
    # Each is processed in alphabetical order of keys, so shorter keys
    # will have their permissions set before longer keys (thus setting
    # the permissions on parent directories before setting permissions
    # on their children).

    # --- FILE PERMISSIONS (Non-created files) --- #
    my %files = (
        '*'               => { perms => OWNER_WRITE },
        # Some .pl files are WS_EXECUTE because we want
        # users to be able to cron them or otherwise run
        # them as a secure user, like the webserver owner.
        '*.cgi'           => { perms => WS_EXECUTE },
        'whineatnews.pl'  => { perms => WS_EXECUTE },
        'collectstats.pl' => { perms => WS_EXECUTE },
        'importxml.pl'    => { perms => WS_EXECUTE },
        'testserver.pl'   => { perms => WS_EXECUTE },
        'whine.pl'        => { perms => WS_EXECUTE },
        'email_in.pl'     => { perms => WS_EXECUTE },
        'sanitycheck.pl'  => { perms => WS_EXECUTE },
        'checksetup.pl'   => { perms => OWNER_EXECUTE },
        'runtests.pl'     => { perms => OWNER_EXECUTE },
        'jobqueue.pl'     => { perms => OWNER_EXECUTE },
        'migrate.pl'      => { perms => OWNER_EXECUTE },
        'install-module.pl' => { perms => OWNER_EXECUTE },
        'clean-bug-user-last-visit.pl' => { perms => WS_EXECUTE },

        'Bugzilla.pm'   => { perms => CGI_READ },
        "$localconfig*" => { perms => CGI_READ },
        'bugzilla.dtd'  => { perms => WS_SERVE },
        'mod_perl.pl'   => { perms => WS_SERVE },
        'robots.txt'    => { perms => WS_SERVE },
        '.htaccess'     => { perms => WS_SERVE },

        'code-review.js'               => { perms => WS_SERVE },    # WEBKIT_CHANGES
        'code-review-test.html'        => { perms => WS_SERVE },    # WEBKIT_CHANGES
        'committers-autocomplete.js'   => { perms => WS_SERVE },    # WEBKIT_CHANGES
        'contrib/recode-overrides.txt' => { perms => OWNER_WRITE }, # WEBKIT_CHANGES

        'contrib/README'       => { perms => OWNER_WRITE },
        'contrib/*/README'     => { perms => OWNER_WRITE },
        'contrib/Bugzilla.pm'  => { perms => OWNER_WRITE },
        'docs/bugzilla.ent'    => { perms => OWNER_WRITE },
        'docs/makedocs.pl'     => { perms => OWNER_EXECUTE },
        'docs/style.css'       => { perms => WS_SERVE },
        'docs/*/rel_notes.txt' => { perms => WS_SERVE },
        'docs/*/README.docs'   => { perms => OWNER_WRITE },
        "$datadir/params.json" => { perms => CGI_WRITE },
        "$datadir/old-params.txt"  => { perms => OWNER_WRITE },
        "$extensionsdir/create.pl" => { perms => OWNER_EXECUTE },
        "$extensionsdir/*/*.pl"    => { perms => WS_EXECUTE },
    );

    # Directories that we want to set the perms on, but not
    # recurse through. These are directories we didn't create
    # in checkesetup.pl.
    my %non_recurse_dirs = (
        '.'  => DIR_WS_SERVE,
        docs => DIR_WS_SERVE,
    );

    # This sets the permissions for each item inside each of these 
    # directories, including the directory itself. 
    # 'CVS' directories are special, though, and are never readable by 
    # the webserver.
    my %recurse_dirs = (
        # Writeable directories
         $template_cache    => { files => CGI_READ,
                                  dirs => DIR_CGI_OVERWRITE },
         $attachdir         => { files => CGI_WRITE,
                                  dirs => DIR_CGI_WRITE },
         $webdotdir         => { files => WS_SERVE,
                                  dirs => DIR_CGI_WRITE | DIR_ALSO_WS_SERVE },
         $graphsdir         => { files => WS_SERVE,
                                  dirs => DIR_CGI_WRITE | DIR_ALSO_WS_SERVE },
         "$datadir/db"      => { files => CGI_WRITE,
                                  dirs => DIR_CGI_WRITE },
         $assetsdir         => { files => WS_SERVE,
                                  dirs => DIR_CGI_OVERWRITE | DIR_ALSO_WS_SERVE },

         # Readable directories
         "$datadir/mining"     => { files => CGI_READ,
                                     dirs => DIR_CGI_READ },
         "$libdir/Bugzilla"    => { files => CGI_READ,
                                     dirs => DIR_CGI_READ },
         $extlib               => { files => CGI_READ,
                                     dirs => DIR_CGI_READ },
         $templatedir          => { files => CGI_READ,
                                     dirs => DIR_CGI_READ },
         # Directories in the extensions/ dir are WS_SERVE so that
         # the web/ directories can be served by the web server.
         # But, for extra security, we deny direct webserver access to
         # the lib/ and template/ directories of extensions.
         $extensionsdir        => { files => CGI_READ,
                                     dirs => DIR_WS_SERVE },
         "$extensionsdir/*/lib" => { files => CGI_READ,
                                      dirs => DIR_CGI_READ },
         "$extensionsdir/*/template" => { files => CGI_READ,
                                           dirs => DIR_CGI_READ },

         # Content served directly by the webserver
         images                => { files => WS_SERVE,
                                     dirs => DIR_WS_SERVE },
         js                    => { files => WS_SERVE,
                                     dirs => DIR_WS_SERVE },
         $skinsdir             => { files => WS_SERVE,
                                     dirs => DIR_WS_SERVE },
         'docs/*/html'         => { files => WS_SERVE,
                                     dirs => DIR_WS_SERVE },
         'docs/*/pdf'          => { files => WS_SERVE,
                                     dirs => DIR_WS_SERVE },
         'docs/*/txt'          => { files => WS_SERVE,
                                     dirs => DIR_WS_SERVE },
         'docs/*/images'       => { files => WS_SERVE,
                                     dirs => DIR_WS_SERVE },
         "$extensionsdir/*/web" => { files => WS_SERVE,
                                     dirs => DIR_WS_SERVE },

         # Directories only for the owner, not for the webserver.
         '.bzr'                => { files => OWNER_WRITE,
                                    dirs  => DIR_OWNER_WRITE },
         t                     => { files => OWNER_WRITE,
                                     dirs => DIR_OWNER_WRITE },
         xt                    => { files => OWNER_WRITE,
                                     dirs => DIR_OWNER_WRITE },
         'docs/lib'            => { files => OWNER_WRITE,
                                     dirs => DIR_OWNER_WRITE },
         'docs/*/xml'          => { files => OWNER_WRITE,
                                     dirs => DIR_OWNER_WRITE },
         'contrib'             => { files => OWNER_EXECUTE,
                                     dirs => DIR_OWNER_WRITE, },
    );

    # --- FILES TO CREATE --- #

    # The name of each directory that we should actually *create*,
    # pointing at its default permissions.
    my %create_dirs = (
        # This is DIR_ALSO_WS_SERVE because it contains $webdotdir and
        # $assetsdir.
        $datadir                => DIR_CGI_OVERWRITE | DIR_ALSO_WS_SERVE,
        # Directories that are read-only for cgi scripts
        "$datadir/mining"       => DIR_CGI_READ,
        "$datadir/extensions"   => DIR_CGI_READ,
        $extensionsdir          => DIR_CGI_READ,
        # Directories that cgi scripts can write to.
        "$datadir/db"           => DIR_CGI_WRITE,
        $attachdir              => DIR_CGI_WRITE,
        $graphsdir              => DIR_CGI_WRITE | DIR_ALSO_WS_SERVE,
        $webdotdir              => DIR_CGI_WRITE | DIR_ALSO_WS_SERVE,
        $assetsdir              => DIR_CGI_WRITE | DIR_ALSO_WS_SERVE,
        # Directories that contain content served directly by the web server.
        "$skinsdir/custom"      => DIR_WS_SERVE,
        "$skinsdir/contrib"     => DIR_WS_SERVE,
    );

    # The name of each file, pointing at its default permissions and
    # default contents.
    my %create_files = (
        "$datadir/extensions/additional" => { perms    => CGI_READ, 
                                              contents => '' },
        # We create this file so that it always has the right owner
        # and permissions. Otherwise, the webserver creates it as
        # owned by itself, which can cause problems if jobqueue.pl
        # or something else is not running as the webserver or root.
        "$datadir/mailer.testfile" => { perms    => CGI_WRITE,
                                        contents => '' },
    );

    # Because checksetup controls the creation of index.html separately
    # from all other files, it gets its very own hash.
    my %index_html = (
        'index.html' => { perms => WS_SERVE, contents => <<EOT
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
  <meta http-equiv="Refresh" content="0; URL=index.cgi">
</head>
<body>
  <h1>I think you are looking for <a href="index.cgi">index.cgi</a></h1>
</body>
</html>
EOT
        }
    );

    # Because checksetup controls the .htaccess creation separately
    # by a localconfig variable, these go in a separate variable from
    # %create_files.
    #
    # Note that these get WS_SERVE as their permission
    # because they're *read* by the webserver, even though they're not
    # actually, themselves, served.
    my %htaccess = (
        "$attachdir/.htaccess"       => { perms    => WS_SERVE,
                                          contents => HT_DEFAULT_DENY },
        "$libdir/Bugzilla/.htaccess" => { perms    => WS_SERVE,
                                          contents => HT_DEFAULT_DENY },
        "$extlib/.htaccess"          => { perms    => WS_SERVE,
                                          contents => HT_DEFAULT_DENY },
        "$templatedir/.htaccess"     => { perms    => WS_SERVE,
                                          contents => HT_DEFAULT_DENY },
        'contrib/.htaccess'          => { perms    => WS_SERVE,
                                          contents => HT_DEFAULT_DENY },
        't/.htaccess'                => { perms    => WS_SERVE,
                                          contents => HT_DEFAULT_DENY },
        'xt/.htaccess'               => { perms    => WS_SERVE,
                                          contents => HT_DEFAULT_DENY },
        "$datadir/.htaccess"         => { perms    => WS_SERVE,
                                          contents => HT_DEFAULT_DENY },

        "$graphsdir/.htaccess" => { perms => WS_SERVE, contents => <<EOT
# Allow access to .png and .gif files.
<FilesMatch (\\.gif|\\.png)\$>
  <IfModule mod_version.c>
    <IfVersion < 2.4>
      Allow from all
    </IfVersion>
    <IfVersion >= 2.4>
      Require all granted
    </IfVersion>
  </IfModule>
  <IfModule !mod_version.c>
    Allow from all
  </IfModule>
</FilesMatch>

# And no directory listings, either.
<IfModule mod_version.c>
  <IfVersion < 2.4>
    Deny from all
  </IfVersion>
  <IfVersion >= 2.4>
    Require all denied
  </IfVersion>
</IfModule>
<IfModule !mod_version.c>
  Deny from all
</IfModule>
EOT
        },

        "$webdotdir/.htaccess" => { perms => WS_SERVE, contents => <<EOT
# Restrict access to .dot files to the public webdot server at research.att.com
# if research.att.com ever changes their IP, or if you use a different
# webdot server, you'll need to edit this
<FilesMatch \\.dot\$>
  <IfModule mod_version.c>
    <IfVersion < 2.4>
      Allow from 192.20.225.0/24
      Deny from all
    </IfVersion>
    <IfVersion >= 2.4>
      Require ip 192.20.225.0/24
      Require all denied
    </IfVersion>
  </IfModule>
  <IfModule !mod_version.c>
    Allow from 192.20.225.0/24
    Deny from all
  </IfModule>
</FilesMatch>

 # Allow access to .png files created by a local copy of 'dot'
 <FilesMatch \\.png\$>
  <IfModule mod_version.c>
    <IfVersion < 2.4>
      Allow from all
    </IfVersion>
    <IfVersion >= 2.4>
      Require all granted
    </IfVersion>
  </IfModule>
  <IfModule !mod_version.c>
    Allow from all
  </IfModule>
</FilesMatch>

# And no directory listings, either.
<IfModule mod_version.c>
  <IfVersion < 2.4>
    Deny from all
  </IfVersion>
  <IfVersion >= 2.4>
    Require all denied
  </IfVersion>
</IfModule>
<IfModule !mod_version.c>
  Deny from all
</IfModule>
EOT
        },

        "$assetsdir/.htaccess" => { perms => WS_SERVE, contents => <<EOT
# Allow access to .css files
<FilesMatch \\.(css|js)\$>
  <IfModule mod_version.c>
    <IfVersion < 2.4>
      Allow from all
    </IfVersion>
    <IfVersion >= 2.4>
      Require all granted
    </IfVersion>
  </IfModule>
  <IfModule !mod_version.c>
     Allow from all
  </IfModule>
</FilesMatch>

# And no directory listings, either.
<IfModule mod_version.c>
  <IfVersion < 2.4>
    Deny from all
  </IfVersion>
  <IfVersion >= 2.4>
    Require all denied
  </IfVersion>
</IfModule>
<IfModule !mod_version.c>
  Deny from all
</IfModule>
EOT
        },

    );

    Bugzilla::Hook::process('install_filesystem', {
        files            => \%files,
        create_dirs      => \%create_dirs,
        non_recurse_dirs => \%non_recurse_dirs,
        recurse_dirs     => \%recurse_dirs,
        create_files     => \%create_files,
        htaccess         => \%htaccess,
    });

    my %all_files = (%create_files, %htaccess, %index_html, %files);
    my %all_dirs  = (%create_dirs, %non_recurse_dirs);

    return {
        create_dirs  => \%create_dirs,
        recurse_dirs => \%recurse_dirs,
        all_dirs     => \%all_dirs,

        create_files => \%create_files,
        htaccess     => \%htaccess,
        index_html   => \%index_html,
        all_files    => \%all_files,
    };
}

sub update_filesystem {
    my ($params) = @_;
    my $fs = FILESYSTEM();
    my %dirs  = %{$fs->{create_dirs}};
    my %files = %{$fs->{create_files}};

    my $datadir = bz_locations->{'datadir'};
    my $graphsdir = bz_locations->{'graphsdir'};
    my $assetsdir = bz_locations->{'assetsdir'};
    # If the graphs/ directory doesn't exist, we're upgrading from
    # a version old enough that we need to update the $datadir/mining 
    # format.
    if (-d "$datadir/mining" && !-d $graphsdir) {
        _update_old_charts($datadir);
    }

    # If there is a file named '-All-' in $datadir/mining, then we're still
    # having mining files named by product name, and we need to convert them to
    # files named by product ID.
    if (-e File::Spec->catfile($datadir, 'mining', '-All-')) {
        _update_old_mining_filenames(File::Spec->catdir($datadir, 'mining'));
    }

    # By sorting the dirs, we assure that shorter-named directories
    # (meaning parent directories) are always created before their
    # child directories.
    foreach my $dir (sort keys %dirs) {
        unless (-d $dir) {
            print "Creating $dir directory...\n";
            mkdir $dir or die "mkdir $dir failed: $!";
            # For some reason, passing in the permissions to "mkdir"
            # doesn't work right, but doing a "chmod" does.
            chmod $dirs{$dir}, $dir or warn "Cannot chmod $dir: $!";
        }
    }

    # Move the testfile if we can't write to it, so that we can re-create
    # it with the correct permissions below.
    my $testfile = "$datadir/mailer.testfile";
    if (-e $testfile and !-w $testfile) {
        _rename_file($testfile, "$testfile.old");
    }

    # If old-params.txt exists in the root directory, move it to datadir.
    my $oldparamsfile = "old_params.txt";
    if (-e $oldparamsfile) {
        _rename_file($oldparamsfile, "$datadir/$oldparamsfile");
    }

    # Remove old assets htaccess file to force recreation with correct values.
    if (-e "$assetsdir/.htaccess") {
        if (read_text("$assetsdir/.htaccess") =~ /<FilesMatch \\\.css\$>/) {
            unlink("$assetsdir/.htaccess");
        }
    }

    _create_files(%files);
    if ($params->{index_html}) {
        _create_files(%{$fs->{index_html}});
    }
    elsif (-e 'index.html') {
        my $templatedir = bz_locations()->{'templatedir'};
        print <<EOT;

*** It appears that you still have an old index.html hanging around.
    Either the contents of this file should be moved into a template and 
    placed in the '$templatedir/en/custom' directory, or you should delete 
    the file.

EOT
    }

    # Delete old files that no longer need to exist

    # 2001-04-29 jake@bugzilla.org - Remove oldemailtech
    #   http://bugzilla.mozilla.org/show_bugs.cgi?id=71552
    if (-d 'shadow') {
        print "Removing shadow directory...\n";
        rmtree("shadow");
    }

    if (-e "$datadir/versioncache") {
        print "Removing versioncache...\n";
        unlink "$datadir/versioncache";
    }

    if (-e "$datadir/duplicates.rdf") {
        print "Removing duplicates.rdf...\n";
        unlink "$datadir/duplicates.rdf";
        unlink "$datadir/duplicates-old.rdf";
    }

    if (-e "$datadir/duplicates") {
        print "Removing duplicates directory...\n";
        rmtree("$datadir/duplicates");
    }

    _remove_empty_css_files();
    _convert_single_file_skins();
    _remove_dynamic_assets();
}

sub _remove_empty_css_files {
    my $skinsdir = bz_locations()->{'skinsdir'};
    foreach my $css_file (glob("$skinsdir/custom/*.css"),
                          glob("$skinsdir/contrib/*/*.css"))
    {
        _remove_empty_css($css_file);
    }
}

# A simple helper for the update code that removes "empty" CSS files.
sub _remove_empty_css {
    my ($file) = @_;
    my $basename = basename($file);
    my $empty_contents = <<EOT;
/*
 * Custom rules for $basename.
 * The rules you put here override rules in that stylesheet.
 */
EOT
    if (length($empty_contents) == -s $file) {
        open(my $fh, '<', $file) or warn "$file: $!";
        my $file_contents;
        { local $/; $file_contents = <$fh>; }
        if ($file_contents eq $empty_contents) {
            print install_string('file_remove', { name => $file }), "\n";
            unlink $file or warn "$file: $!";
        }
    };
}

# We used to allow a single css file in the skins/contrib/ directory
# to be a whole skin.
sub _convert_single_file_skins {
    my $skinsdir = bz_locations()->{'skinsdir'};
    foreach my $skin_file (glob "$skinsdir/contrib/*.css") {
        my $dir_name = $skin_file;
        $dir_name =~ s/\.css$//;
        mkdir $dir_name or warn "$dir_name: $!";
        _rename_file($skin_file, "$dir_name/global.css");
    }
}

# delete all automatically generated css/js files to force recreation at the
# next request.
sub _remove_dynamic_assets {
    my @files = (
        glob(bz_locations()->{assetsdir} . '/*.css'),
        glob(bz_locations()->{assetsdir} . '/*.js'),
    );
    foreach my $file (@files) {
        unlink($file);
    }

    # remove old skins/assets directory
    my $old_path = bz_locations()->{skinsdir} . '/assets';
    if (-d $old_path) {
        foreach my $file (glob("$old_path/*.css")) {
            unlink($file);
        }
        rmdir($old_path);
    }
}

sub create_htaccess {
    _create_files(%{FILESYSTEM()->{htaccess}});

    # Repair old .htaccess files

    my $webdot_dir = bz_locations()->{'webdotdir'};
    # The public webdot IP address changed.
    my $webdot = new IO::File("$webdot_dir/.htaccess", 'r')
        || die "$webdot_dir/.htaccess: $!";
    my $webdot_data;
    { local $/; $webdot_data = <$webdot>; }
    $webdot->close;
    if ($webdot_data =~ /192\.20\.225\.10/) {
        print "Repairing $webdot_dir/.htaccess...\n";
        $webdot_data =~ s/192\.20\.225\.10/192.20.225.0\/24/g;
        $webdot = new IO::File("$webdot_dir/.htaccess", 'w') || die $!;
        print $webdot $webdot_data;
        $webdot->close;
    }
}

sub _rename_file {
    my ($from, $to) = @_;
    print install_string('file_rename', { from => $from, to => $to }), "\n";
    if (-e $to) {
        warn "$to already exists, not moving\n";
    }
    else {
        move($from, $to) or warn $!;
    }
}

# A helper for the above functions.
sub _create_files {
    my (%files) = @_;

    # It's not necessary to sort these, but it does make the
    # output of checksetup.pl look a bit nicer.
    foreach my $file (sort keys %files) {
        unless (-e $file) {
            print "Creating $file...\n";
            my $info = $files{$file};
            my $fh = new IO::File($file, O_WRONLY | O_CREAT, $info->{perms})
                || die $!;
            print $fh $info->{contents} if $info->{contents};
            $fh->close;
        }
    }
}

# If you ran a REALLY old version of Bugzilla, your chart files are in the
# wrong format. This code is a little messy, because it's very old, and
# when moving it into this module, I couldn't test it so I left it almost 
# completely alone.
sub _update_old_charts {
    my ($datadir) = @_;
    print "Updating old chart storage format...\n";
    foreach my $in_file (glob("$datadir/mining/*")) {
        # Don't try and upgrade image or db files!
        next if (($in_file =~ /\.gif$/i) ||
                 ($in_file =~ /\.png$/i) ||
                 ($in_file =~ /\.db$/i) ||
                 ($in_file =~ /\.orig$/i));

        rename("$in_file", "$in_file.orig") or next;
        open(IN, "<", "$in_file.orig") or next;
        open(OUT, '>', $in_file) or next;

        # Fields in the header
        my @declared_fields;

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
                my @data = split(/\||\r/, $1);
                my %data;
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
                          scalar(@data) . " fields\nThis was unexpected.",
                          " You may want to check your data files.\n";
                }

                print OUT join('|', 
                    map { defined ($data{$_}) ? ($data{$_}) : "" } @out_fields),
                    "\n";
            }
            else {
                print OUT;
            }
        }

        close(IN);
        close(OUT);
    } 
}

# The old naming scheme has product names as mining file names; we rename them
# to product IDs.
sub _update_old_mining_filenames {
    my ($miningdir) = @_;
    my $dbh = Bugzilla->dbh;
    my @conversion_errors;

    # We use a dummy product instance with ID 0, representing all products
    my $product_all = {id => 0, name => '-All-'};

    print "Updating old charting data file names...";
    my @products = @{ $dbh->selectall_arrayref('SELECT id, name FROM products
                                                ORDER BY name', {Slice=>{}}) };
    push(@products, $product_all);
    foreach my $product (@products) {
        if (-e File::Spec->catfile($miningdir, $product->{id})) {
            push(@conversion_errors,
                 { product => $product,
                   message => 'A file named "' . $product->{id} .
                              '" already exists.' });
        }
    }

    if (! @conversion_errors) {
        # Renaming mining files should work now without a hitch.
        foreach my $product (@products) {
            if (! rename(File::Spec->catfile($miningdir, $product->{name}),
                         File::Spec->catfile($miningdir, $product->{id}))) {
                push(@conversion_errors,
                     { product => $product,
                       message => $! });
            }
        }
    }

    # Error reporting
    if (! @conversion_errors) {
        print " done.\n";
    }
    else {
        print " FAILED:\n";
        foreach my $error (@conversion_errors) {
            printf "Cannot rename charting data file for product %d (%s): %s\n",
                   $error->{product}->{id}, $error->{product}->{name},
                   $error->{message};
        }
        print "You need to empty the \"$miningdir\" directory, then run\n",
              "   collectstats.pl --regenerate\n",
              "in order to clean this up.\n";
    }
}

sub fix_dir_permissions {
    my ($dir) = @_;
    return if ON_WINDOWS;
    # Note that _get_owner_and_group is always silent here.
    my ($owner_id, $group_id) = _get_owner_and_group();

    my $perms;
    my $fs = FILESYSTEM();
    if ($perms = $fs->{recurse_dirs}->{$dir}) {
        _fix_perms_recursively($dir, $owner_id, $group_id, $perms);
    }
    elsif ($perms = $fs->{all_dirs}->{$dir}) {
        _fix_perms($dir, $owner_id, $group_id, $perms);
    }
    else {
        # Do nothing. We know nothing about this directory.
        warn "Unknown directory $dir";
    }
}

sub fix_file_permissions {
    my ($file) = @_;
    return if ON_WINDOWS;
    my $perms = FILESYSTEM()->{all_files}->{$file}->{perms};
    # Note that _get_owner_and_group is always silent here.
    my ($owner_id, $group_id) = _get_owner_and_group();
    _fix_perms($file, $owner_id, $group_id, $perms);
}

sub fix_all_file_permissions {
    my ($output) = @_;

    # _get_owner_and_group also checks that the webservergroup is valid.
    my ($owner_id, $group_id) = _get_owner_and_group($output);

    return if ON_WINDOWS;

    my $fs = FILESYSTEM();
    my %files = %{$fs->{all_files}};
    my %dirs  = %{$fs->{all_dirs}};
    my %recurse_dirs = %{$fs->{recurse_dirs}};

    print get_text('install_file_perms_fix') . "\n" if $output;

    foreach my $dir (sort keys %dirs) {
        next unless -d $dir;
        _fix_perms($dir, $owner_id, $group_id, $dirs{$dir});
    }

    foreach my $pattern (sort keys %recurse_dirs) {
        my $perms = $recurse_dirs{$pattern};
        # %recurse_dirs supports globs
        foreach my $dir (glob $pattern) {
            next unless -d $dir;
            _fix_perms_recursively($dir, $owner_id, $group_id, $perms);
        }
    }

    foreach my $file (sort keys %files) {
        # %files supports globs
        foreach my $filename (glob $file) {
            # Don't touch directories.
            next if -d $filename || !-e $filename;
            _fix_perms($filename, $owner_id, $group_id, 
                       $files{$file}->{perms});
        }
    }

    _fix_cvs_dirs($owner_id, '.');
}

sub _get_owner_and_group {
    my ($output) = @_;
    my $group_id = _check_web_server_group($output);
    return () if ON_WINDOWS;

    my $owner_id = POSIX::getuid();
    $group_id = POSIX::getgid() unless defined $group_id;
    return ($owner_id, $group_id);
}

# A helper for fix_all_file_permissions
sub _fix_cvs_dirs {
    my ($owner_id, $dir) = @_;
    my $owner_gid = POSIX::getgid();
    find({ no_chdir => 1, wanted => sub {
        my $name = $File::Find::name;
        if ($File::Find::dir =~ /\/CVS/ || $_ eq '.cvsignore'
            || (-d $name && $_ =~ /CVS$/)) 
        {
            my $perms = 0600;
            if (-d $name) {
                $perms = 0700;
            }
            _fix_perms($name, $owner_id, $owner_gid, $perms);
        }
    }}, $dir);
}

sub _fix_perms {
    my ($name, $owner, $group, $perms) = @_;
    #printf ("Changing $name to %o\n", $perms);

    # The webserver should never try to chown files.
    if (Bugzilla->usage_mode == USAGE_MODE_CMDLINE) {
        chown $owner, $group, $name
            or warn install_string('chown_failed', { path => $name, 
                                                     error => $! }) . "\n";
    }
    chmod $perms, $name
        or warn install_string('chmod_failed', { path => $name, 
                                                 error => $! }) . "\n";
}

sub _fix_perms_recursively {
    my ($dir, $owner_id, $group_id, $perms) = @_;
    # Set permissions on the directory itself.
    _fix_perms($dir, $owner_id, $group_id, $perms->{dirs});
    # Now recurse through the directory and set the correct permissions
    # on subdirectories and files.
    find({ no_chdir => 1, wanted => sub {
        my $name = $File::Find::name;
        if (-d $name) {
            _fix_perms($name, $owner_id, $group_id, $perms->{dirs});
        }
        else {
            _fix_perms($name, $owner_id, $group_id, $perms->{files});
        }
    }}, $dir);
}

sub _check_web_server_group {
    my ($output) = @_;

    my $group    = Bugzilla->localconfig->{'webservergroup'};
    my $filename = bz_locations()->{'localconfig'};
    my $group_id;

    # If we are on Windows, webservergroup does nothing
    if (ON_WINDOWS && $group && $output) {
        print "\n\n" . get_text('install_webservergroup_windows') . "\n\n";
    }

    # If we're not on Windows, make sure that webservergroup isn't
    # empty.
    elsif (!ON_WINDOWS && !$group && $output) {
        print "\n\n" . get_text('install_webservergroup_empty') . "\n\n";
    }

    # If we're not on Windows, make sure we are actually a member of
    # the webservergroup.
    elsif (!ON_WINDOWS && $group) {
        $group_id = getgrnam($group);
        ThrowCodeError('invalid_webservergroup', { group => $group }) 
            unless defined $group_id;

        # If on unix, see if we need to print a warning about a webservergroup
        # that we can't chgrp to
        if ($output && $< != 0 && !grep($_ eq $group_id, split(" ", $)))) {
            print "\n\n" . get_text('install_webservergroup_not_in') . "\n\n";
        }
    }

    return $group_id;
}

1;

__END__

=head1 NAME

Bugzilla::Install::Filesystem - Fix up the filesystem during
  installation.

=head1 DESCRIPTION

This module is used primarily by L<checksetup.pl> to modify the 
filesystem during installation, including creating the data/ directory.

=head1 SUBROUTINES

=over

=item C<update_filesystem({ index_html => 0 })>

Description: Creates all the directories and files that Bugzilla
             needs to function but doesn't ship with. Also does
             any updates to these files as necessary during an
             upgrade.

Params:      C<index_html> - Whether or not we should create
               the F<index.html> file.

Returns:     nothing

=item C<create_htaccess()>

Description: Creates all of the .htaccess files for Apache,
             in the various Bugzilla directories. Also updates
             the .htaccess files if they need updating.

Params:      none

Returns:     nothing

=item C<fix_all_file_permissions($output)>

Description: Sets all the file permissions on all of Bugzilla's files
             to what they should be. Note that permissions are different
             depending on whether or not C<$webservergroup> is set
             in F<localconfig>.

Params:      C<$output> - C<true> if you want this function to print
                 out information about what it's doing.

Returns:     nothing

=item C<fix_dir_permissions>

Given the name of a directory, its permissions will be fixed according to
how they are supposed to be set in Bugzilla's current configuration.
If it fails to set the permissions, a warning will be printed to STDERR.

=item C<fix_file_permissions>

Given the name of a file, its permissions will be fixed according to
how they are supposed to be set in Bugzilla's current configuration.
If it fails to set the permissions, a warning will be printed to STDERR.

=back

=head1 B<Methods in need of POD>

=over

=item CGI_WRITE

=item DIR_WS_SERVE

=item DIR_ALSO_WS_SERVE

=item WS_SERVE

=item FILESYSTEM

=item WS_EXECUTE

=item CGI_READ

=item DIR_CGI_READ

=item DIR_CGI_WRITE

=item DIR_CGI_OVERWRITE

=back
