#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util qw(trim);

use File::Basename;
use File::Copy qw(move);
use File::Find;
use File::Path qw(mkpath rmtree);

my $from = $ARGV[0]
  or die <<END;
You must specify the name of the extension you are converting from,
as the first argument.
END
my $extension_name = ucfirst($from);

my $extdir = bz_locations()->{'extensionsdir'};

my $from_dir = "$extdir/$from";
if (!-d $from_dir) {
    die "$from_dir does not exist.\n";
}

my $to_dir = "$extdir/$extension_name";
if (-d $to_dir) {
    die "$to_dir already exists, not converting.\n";
}

if (ON_WINDOWS) {
    # There's no easy way to recursively copy a directory on Windows.
    print "WARNING: This will modify the contents of $from_dir.\n",
          "Press Ctrl-C to stop or any other key to continue...\n";
    getc;
    move($from_dir, $to_dir) 
        || die "rename of $from_dir to $to_dir failed: $!";
}
else {
    print "Copying $from_dir to $to_dir...\n";
    system("cp", "-r", $from_dir, $to_dir);
}

# Make sure we don't accidentally modify the $from_dir anywhere else 
# in this script.
undef $from_dir;

if (!-d $to_dir) {
    die "$to_dir was not created.\n";
}

my $version = get_version($to_dir);
move_template_hooks($to_dir);
rename_module_packages($to_dir, $extension_name);
my $install_requirements = get_install_requirements($to_dir);
my ($modules, $subs) = code_files_to_subroutines($to_dir);

my $config_pm = <<END;
package Bugzilla::Extension::$extension_name;
use strict;
use warnings;

use constant NAME => '$extension_name';
$install_requirements
__PACKAGE__->NAME;
END

my $extension_pm = <<END;
package Bugzilla::Extension::$extension_name;
use strict;
use warnings;

use parent qw(Bugzilla::Extension);

$modules

our \$VERSION = '$version';

$subs

__PACKAGE__->NAME;
END

open(my $config_fh, '>', "$to_dir/Config.pm") || die "$to_dir/Config.pm: $!";
print $config_fh $config_pm;
close($config_fh);
open(my $extension_fh, '>', "$to_dir/Extension.pm") 
  || die "$to_dir/Extension.pm: $!";
print $extension_fh $extension_pm;
close($extension_fh);

rmtree("$to_dir/code");
unlink("$to_dir/info.pl");

###############
# Subroutines #
###############

sub rename_module_packages {
    my ($dir, $name) = @_;
    my $lib_dir = "$dir/lib";

    # We don't want things like Bugzilla::Extension::Testopia::Testopia.
    if (-d "$lib_dir/$name") {
        print "Moving contents of $lib_dir/$name into $lib_dir...\n";
        foreach my $file (glob("$lib_dir/$name/*")) {
            my $dirname = dirname($file);
            my $basename = basename($file);
            rename($file, "$dirname/../$basename") || warn "$file: $!\n";
        }
    }

    my @modules;
    find({ wanted   => sub { $_ =~ /\.pm$/i and push(@modules, $_) }, 
           no_chdir => 1 }, $lib_dir);
    my %module_rename;
    foreach my $file (@modules) {
        open(my $fh, '<', $file) || die "$file: $!";
        my $content = do { local $/ = undef; <$fh> };
        close($fh);
        if ($content =~ /^package (\S+);/m) {
            my $package = $1;
            my $new_name = $file;
            $new_name =~ s/^$lib_dir\///;
            $new_name =~ s/\.pm$//;
            $new_name = join('::', File::Spec->splitdir($new_name));
            $new_name = "Bugzilla::Extension::${name}::$new_name";
            print "Renaming $package to $new_name...\n";
            $content =~ s/^package \Q$package\E;/package \Q$new_name\E;/;
            open(my $write_fh, '>', $file) || die "$file: $!";
            print $write_fh $content;
            close($write_fh);
            $module_rename{$package} = $new_name;
        }
    }

    print "Renaming module names inside of library and code files...\n";
    my @code_files = glob("$dir/code/*.pl");
    rename_modules_internally(\%module_rename, [@modules, @code_files]);
}

sub rename_modules_internally {
    my ($rename, $files) = @_;

    # We can't use \b because :: matches \b.
    my $break = qr/^|[^\w:]|$/;
    foreach my $file (@$files) {
        open(my $fh, '<', $file) || die "$file: $!";
        my $content = do { local $/ = undef; <$fh> };
        close($fh);
        foreach my $old_name (keys %$rename) {
            my $new_name = $rename->{$old_name};
            $content =~ s/($break)\Q$old_name\E($break)/$1$new_name$2/gms;
        }
        open(my $write_fh, '>', $file) || die "$file: $!";
        print $write_fh $content;
        close($write_fh);
    }
}

sub get_version {
    my ($dir) = @_;
    print "Getting version info from info.pl...\n";
    my $info;
    {
        local @INC = ("$dir/lib", @INC);
        $info = do "$dir/info.pl"; die $@ if $@;
    }
    return $info->{version};
}

sub get_install_requirements {
    my ($dir) = @_;
    my $file = "$dir/code/install-requirements.pl";
    return '' if !-f $file;

    print "Moving install-requirements.pl code into Config.pm...\n";
    my ($modules, $code) = process_code_file($file);
    $modules = join('', @$modules);
    $code = join('', @$code);
    if ($modules) {
        return "$modules\n\n$code";
    }
    return $code;
}

sub process_code_file {
    my ($file) = @_;
    open(my $fh, '<', $file) || die "$file: $!";
    my $stuff_started;
    my (@modules, @code);
    foreach my $line (<$fh>) {
        $stuff_started = 1 if $line !~ /^#/;
        next if !$stuff_started;
        next if $line =~ /^use (warnings|strict|lib|Bugzilla)[^\w:]/;
        if ($line =~ /^(?:use|require)\b/) {
            push(@modules, $line);
        }
        else {
            push(@code, $line);
        }
    }
    close $fh;
    return (\@modules, \@code);
}

sub code_files_to_subroutines {
    my ($dir) = @_;

    my @dir_files = glob("$dir/code/*.pl");
    my (@all_modules, @subroutines);
    foreach my $file (@dir_files) {
        next if $file =~ /install-requirements/;
        print "Moving $file code into Extension.pm...\n";
        my ($modules, $code) = process_code_file($file);
        my @code_lines = map { "    $_" } @$code;
        my $code_string = join('', @code_lines);
        $code_string =~ s/Bugzilla->hook_args/\$args/g;
        $code_string =~ s/my\s+\$args\s+=\s+\$args;//gs;
        chomp($code_string);
        push(@all_modules, @$modules);
        my $name = basename($file);
        $name =~ s/-/_/;
        $name =~ s/\.pl$//;

        my $subroutine = <<END;
sub $name {
    my (\$self, \$args) = \@_;
$code_string
}
END
        push(@subroutines, $subroutine);
    }

    my %seen_modules = map { trim($_) => 1 } @all_modules;
    my $module_string = join("\n", sort keys %seen_modules);
    my $subroutine_string = join("\n", @subroutines);
    return ($module_string, $subroutine_string);
}

sub move_template_hooks {
    my ($dir) = @_;
    foreach my $lang (glob("$dir/template/*")) {
        next if !_file_matters($lang);
        my $hook_container = "$lang/default/hook";
        mkpath($hook_container) || warn "$hook_container: $!";
        # Hooks can be in all sorts of weird places, including
        # template/default/hook.
        foreach my $file (glob("$lang/*")) {
            next if !_file_matters($file, 1);
            my $dirname = basename($file);
            print "Moving $file to $hook_container/$dirname...\n";
            rename($file, "$hook_container/$dirname") || die "move failed: $!";
        }
    }
}

sub _file_matters {
     my ($path, $tmpl) = @_;
     my @ignore = qw(default custom CVS);
     my $file = basename($path);
     return 0 if grep(lc($_) eq lc($file), @ignore);
      # Hidden files
     return 0 if $file =~ /^\./;
     if ($tmpl) {
         return 1 if $file =~ /\.tmpl$/;
     }
     return 0 if !-d $path;
     return 1;
}

__END__

=head1 NAME

extension-convert.pl - Convert extensions from the pre-3.6 format to the 
3.6 format.

=head1 SYNOPSIS

 contrib/extension-convert.pl name

 Converts an extension in the F<extensions/> directory into the new
 extension layout for Bugzilla 3.6.
