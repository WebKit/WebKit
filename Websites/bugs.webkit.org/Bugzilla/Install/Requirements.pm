# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Install::Requirements;

# NOTE: This package MUST NOT "use" any Bugzilla modules other than
# Bugzilla::Constants, anywhere. We may "use" standard perl modules.
#
# Subroutines may "require" and "import" from modules, but they
# MUST NOT "use."

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Constants;
use Bugzilla::Install::Util qw(install_string bin_loc
                               extension_requirement_packages);
use List::Util qw(max);
use Term::ANSIColor;

use parent qw(Exporter);
our @EXPORT = qw(
    REQUIRED_MODULES
    OPTIONAL_MODULES
    FEATURE_FILES

    check_requirements
    check_graphviz
    have_vers
    install_command
    map_files_to_features
);

# This is how many *'s are in the top of each "box" message printed
# by checksetup.pl.
use constant TABLE_WIDTH => 71;

# Optional Apache modules that have no Perl component to them.
# If these are installed, Bugzilla has additional functionality.
#
# The keys are the names of the modules, the values are what the module
# is called in the output of "apachectl -t -D DUMP_MODULES".
use constant APACHE_MODULES => { 
    mod_headers => 'headers_module',
    mod_env     => 'env_module',
    mod_expires => 'expires_module',
    mod_rewrite => 'rewrite_module',
    mod_version => 'version_module'
};

# These are all of the binaries that we could possibly use that can
# give us info about which Apache modules are installed.
# If we can't use "apachectl", the "httpd" binary itself takes the same
# parameters. Note that on Debian and Gentoo, there is an "apache2ctl",
# but it takes different parameters on each of those two distros, so we
# don't use apache2ctl.
use constant APACHE => qw(apachectl httpd apache2 apache);

# If we don't find any of the above binaries in the normal PATH,
# these are extra places we look.
use constant APACHE_PATH => [qw(
    /usr/sbin 
    /usr/local/sbin
    /usr/libexec
    /usr/local/libexec
)];

# The below two constants are subroutines so that they can implement
# a hook. Other than that they are actually constants.

# "package" is the perl package we're checking for. "module" is the name
# of the actual module we load with "require" to see if the package is
# installed or not. "version" is the version we need, or 0 if we'll accept
# any version.
#
# "blacklist" is an arrayref of regular expressions that describe versions that
# are 'blacklisted'--that is, even if the version is high enough, Bugzilla
# will refuse to say that it's OK to run with that version.
sub REQUIRED_MODULES {
    my @modules = (
    {
        package => 'CGI.pm',
        module  => 'CGI',
        # 3.51 fixes a security problem that affects Bugzilla.
        # (bug 591165)
        version => '3.51',
    },
    {
        package => 'Digest-SHA',
        module  => 'Digest::SHA',
        version => 0
    },
    # 0.23 fixes incorrect handling of 1/2 & 3/4 timezones.
    {
        package => 'TimeDate',
        module  => 'Date::Format',
        version => '2.23'
    },
    # 0.75 fixes a warning thrown with Perl 5.17 and newer.
    {
        package => 'DateTime',
        module  => 'DateTime',
        version => '0.75'
    },
    # 1.64 fixes a taint issue preventing the local timezone from
    # being determined on some systems.
    {
        package => 'DateTime-TimeZone',
        module  => 'DateTime::TimeZone',
        version => '1.64'
    },
    # 1.54 is required for Perl 5.10+. It also makes DBD::Oracle happy.
    {
        package => 'DBI',
        module  => 'DBI',
        version => ($^V >= v5.13.3) ? '1.614' : '1.54'
    },
    # 2.24 contains several useful text virtual methods.
    {
        package => 'Template-Toolkit',
        module  => 'Template',
        version => '2.24'
    },
    # 1.300011 has a debug mode for SMTP and automatically pass -i to sendmail.
    {
        package => 'Email-Sender',
        module  => 'Email::Sender',
        version => '1.300011',
    },
    {
        package => 'Email-MIME',
        module  => 'Email::MIME',
        # This fixes a memory leak in walk_parts that affected jobqueue.pl.
        version => '1.904'
    },
    {
        package => 'URI',
        module  => 'URI',
        # Follows RFC 3986 to escape characters in URI::Escape.
        version => '1.55',
    },
    # 0.32 fixes several memory leaks in the XS version of some functions.
    {
        package => 'List-MoreUtils',
        module  => 'List::MoreUtils',
        version => 0.32,
    },
    {
        package => 'Math-Random-ISAAC',
        module  => 'Math::Random::ISAAC',
        version => '1.0.1',
    },
    {
        package => 'JSON-XS',
        module  => 'JSON::XS',
        # 2.0 is the first version that will work with JSON::RPC.
        version => '2.01',
    },
    );

    if (ON_WINDOWS) {
        push(@modules,
        {
            package => 'Win32',
            module  => 'Win32',
            # 0.35 fixes a memory leak in GetOSVersion, which we use.
            version => 0.35,
        }, 
        {
            package => 'Win32-API',
            module  => 'Win32::API',
            # 0.55 fixes a bug with char* that might affect Bugzilla::RNG.
            version => '0.55',
        },
        {
            package => 'DateTime-TimeZone-Local-Win32',
            module  => 'DateTime::TimeZone::Local::Win32',
            # We require DateTime::TimeZone 1.64, so this version must match.
            version => '1.64',
        }
        );
    }

    my $extra_modules = _get_extension_requirements('REQUIRED_MODULES');
    push(@modules, @$extra_modules);
    return \@modules;
};

sub OPTIONAL_MODULES {
    my @modules = (
    {
        package => 'GD',
        module  => 'GD',
        version => '1.20',
        feature => [qw(graphical_reports new_charts old_charts)],
    },
    {
        package => 'Chart',
        module  => 'Chart::Lines',
        # Versions below 2.4.1 cannot be compared accurately, see
        # https://rt.cpan.org/Public/Bug/Display.html?id=28218.
        version => '2.4.1',
        feature => [qw(new_charts old_charts)],
    },
    {
        package => 'Template-GD',
        # This module tells us whether or not Template-GD is installed
        # on Template-Toolkits after 2.14, and still works with 2.14 and lower.
        module  => 'Template::Plugin::GD::Image',
        version => 0,
        feature => ['graphical_reports'],
    },
    {
        package => 'GDTextUtil',
        module  => 'GD::Text',
        version => 0,
        feature => ['graphical_reports'],
    },
    {
        package => 'GDGraph',
        module  => 'GD::Graph',
        version => 0,
        feature => ['graphical_reports'],
    },
    {
        package => 'MIME-tools',
        # MIME::Parser is packaged as MIME::Tools on ActiveState Perl
        module  => ON_WINDOWS ? 'MIME::Tools' : 'MIME::Parser',
        version => '5.406',
        feature => ['moving'],
    },
    {
        package => 'libwww-perl',
        module  => 'LWP::UserAgent',
        version => 0,
        feature => ['updates'],
    },
    {
        package => 'XML-Twig',
        module  => 'XML::Twig',
        version => 0,
        feature => ['moving', 'updates'],
    },
    {
        package => 'PatchReader',
        module  => 'PatchReader',
        # 0.9.6 fixes two notable bugs and significantly improves the UX.
        version => '0.9.6',
        feature => ['patch_viewer'],
    },
    {
        package => 'perl-ldap',
        module  => 'Net::LDAP',
        version => 0,
        feature => ['auth_ldap'],
    },
    {
        package => 'Authen-SASL',
        module  => 'Authen::SASL',
        version => 0,
        feature => ['smtp_auth'],
    },
    {
        package => 'Net-SMTP-SSL',
        module  => 'Net::SMTP::SSL',
        version => 1.01,
        feature => ['smtp_ssl'],
    },
    {
        package => 'RadiusPerl',
        module  => 'Authen::Radius',
        version => 0,
        feature => ['auth_radius'],
    },
    # XXX - Once we require XMLRPC::Lite 0.717 or higher, we can
    # remove SOAP::Lite from the list.
    {
        package => 'SOAP-Lite',
        module  => 'SOAP::Lite',
        # Fixes various bugs, including 542931 and 552353 + stops
        # throwing warnings with Perl 5.12.
        version => '0.712',
        # SOAP::Transport::HTTP 1.12 is bogus.
        blacklist => ['^1\.12$'],
        feature => ['xmlrpc'],
    },
    # Since SOAP::Lite 1.0, XMLRPC::Lite is no longer included
    # and so it must be checked separately.
    {
        package => 'XMLRPC-Lite',
        module  => 'XMLRPC::Lite',
        version => '0.712',
        feature => ['xmlrpc'],
    },
    {
        package => 'JSON-RPC',
        module  => 'JSON::RPC',
        version => 0,
        feature => ['jsonrpc', 'rest'],
    },
    {
        package => 'Test-Taint',
        module  => 'Test::Taint',
        # 1.06 no longer throws warnings with Perl 5.10+.
        version => 1.06,
        feature => ['jsonrpc', 'xmlrpc', 'rest'],
    },
    {
        # We need the 'utf8_mode' method of HTML::Parser, for HTML::Scrubber.
        package => 'HTML-Parser',
        module  => 'HTML::Parser',
        version => ($^V >= v5.13.3) ? '3.67' : '3.40',
        feature => ['html_desc'],
    },
    {
        package => 'HTML-Scrubber',
        module  => 'HTML::Scrubber',
        version => 0,
        feature => ['html_desc'],
    },
    {
        # we need version 2.21 of Encode for mime_name
        package => 'Encode',
        module  => 'Encode',
        version => 2.21,
        feature => ['detect_charset'],
    },
    {
        package => 'Encode-Detect',
        module  => 'Encode::Detect',
        version => 0,
        feature => ['detect_charset'],
    },

    # Inbound Email
    {
        package => 'Email-Reply',
        module  => 'Email::Reply',
        version => 0,
        feature => ['inbound_email'],
    },
    {
        package => 'HTML-FormatText-WithLinks',
        module  => 'HTML::FormatText::WithLinks',
        # We need 0.13 to set the "bold" marker to "*".
        version => '0.13',
        feature => ['inbound_email'],
    },

    # Mail Queueing
    {
        package => 'TheSchwartz',
        module  => 'TheSchwartz',
        # 1.07 supports the prioritization of jobs.
        version => 1.07,
        feature => ['jobqueue'],
    },
    {
        package => 'Daemon-Generic',
        module  => 'Daemon::Generic',
        version => 0,
        feature => ['jobqueue'],
    },

    # mod_perl
    {
        package => 'mod_perl',
        module  => 'mod_perl2',
        version => '1.999022',
        feature => ['mod_perl'],
    },
    {
        package => 'Apache-SizeLimit',
        module  => 'Apache2::SizeLimit',
        # 0.96 properly determines process size on Linux.
        version => '0.96',
        feature => ['mod_perl'],
    },

    # typesniffer
    {
        package => 'File-MimeInfo',
        module  => 'File::MimeInfo::Magic',
        version => '0',
        feature => ['typesniffer'],
    },
    {
        package => 'IO-stringy',
        module  => 'IO::Scalar',
        version => '0',
        feature => ['typesniffer'],
    },

    # memcached
    {
        package => 'Cache-Memcached',
        module  => 'Cache::Memcached',
        version => '0',
        feature => ['memcached'],
    },

    # Documentation
    {
        package => 'File-Copy-Recursive',
        module  => 'File::Copy::Recursive',
        version => 0,
        feature => ['documentation'],
    },
    {
        package => 'File-Which',
        module  => 'File::Which',
        version => 0,
        feature => ['documentation'],
    },
    );

    my $extra_modules = _get_extension_requirements('OPTIONAL_MODULES');
    push(@modules, @$extra_modules);
    return \@modules;
};

# This maps features to the files that require that feature in order
# to compile. It is used by t/001compile.t and mod_perl.pl.
use constant FEATURE_FILES => (
    jsonrpc       => ['Bugzilla/WebService/Server/JSONRPC.pm', 'jsonrpc.cgi'],
    xmlrpc        => ['Bugzilla/WebService/Server/XMLRPC.pm', 'xmlrpc.cgi',
                      'Bugzilla/WebService.pm', 'Bugzilla/WebService/*.pm'],
    rest          => ['Bugzilla/WebService/Server/REST.pm', 'rest.cgi',
                      'Bugzilla/WebService/Server/REST/Resources/*.pm'],
    moving        => ['importxml.pl'],
    auth_ldap     => ['Bugzilla/Auth/Verify/LDAP.pm'],
    auth_radius   => ['Bugzilla/Auth/Verify/RADIUS.pm'],
    documentation => ['docs/makedocs.pl'],
    inbound_email => ['email_in.pl'],
    jobqueue      => ['Bugzilla/Job/*', 'Bugzilla/JobQueue.pm',
                      'Bugzilla/JobQueue/*', 'jobqueue.pl'],
    patch_viewer  => ['Bugzilla/Attachment/PatchReader.pm'],
    updates       => ['Bugzilla/Update.pm'],
    memcached     => ['Bugzilla/Memcache.pm'],
);

# This implements the REQUIRED_MODULES and OPTIONAL_MODULES stuff
# described in in Bugzilla::Extension.
sub _get_extension_requirements {
    my ($function) = @_;

    my $packages = extension_requirement_packages();
    my @modules;
    foreach my $package (@$packages) {
        if ($package->can($function)) {
            my $extra_modules = $package->$function;
            push(@modules, @$extra_modules);
        }
    }
    return \@modules;
};

sub check_requirements {
    my ($output) = @_;

    print "\n", install_string('checking_modules'), "\n" if $output;
    my $root = ROOT_USER;
    my $missing = _check_missing(REQUIRED_MODULES, $output);

    print "\n", install_string('checking_dbd'), "\n" if $output;
    my $have_one_dbd = 0;
    my $db_modules = DB_MODULE;
    foreach my $db (keys %$db_modules) {
        my $dbd = $db_modules->{$db}->{dbd};
        $have_one_dbd = 1 if have_vers($dbd, $output);
    }

    print "\n", install_string('checking_optional'), "\n" if $output;
    my $missing_optional = _check_missing(OPTIONAL_MODULES, $output);

    my $missing_apache = _missing_apache_modules(APACHE_MODULES, $output);

    # If we're running on Windows, reset the input line terminator so that
    # console input works properly - loading CGI tends to mess it up
    $/ = "\015\012" if ON_WINDOWS;

    my $pass = !scalar(@$missing) && $have_one_dbd;
    return {
        pass     => $pass,
        one_dbd  => $have_one_dbd,
        missing  => $missing,
        optional => $missing_optional,
        apache   => $missing_apache,
        any_missing => !$pass || scalar(@$missing_optional),
    };
}

# A helper for check_requirements
sub _check_missing {
    my ($modules, $output) = @_;

    my @missing;
    foreach my $module (@$modules) {
        unless (have_vers($module, $output)) {
            push(@missing, $module);
        }
    }

    return \@missing;
}

sub _missing_apache_modules {
    my ($modules, $output) = @_;
    my $apachectl = _get_apachectl();
    return [] if !$apachectl;
    my $command = "$apachectl -t -D DUMP_MODULES";
    my $cmd_info = `$command 2>&1`;
    # If apachectl returned a value greater than 0, then there was an
    # error parsing Apache's configuration, and we can't check modules.
    my $retval = $?;
    if ($retval > 0) {
        print STDERR install_string('apachectl_failed', 
            { command => $command, root => ROOT_USER }), "\n";
        return [];
    }
    my @missing;
    foreach my $module (sort keys %$modules) {
        my $ok = _check_apache_module($module, $modules->{$module}, 
                                      $cmd_info, $output);
        push(@missing, $module) if !$ok;
    }
    return \@missing;
}

sub _get_apachectl {
    foreach my $bin_name (APACHE) {
        my $bin = bin_loc($bin_name);
        return $bin if $bin;
    }
    # Try again with a possibly different path.
    foreach my $bin_name (APACHE) {
        my $bin = bin_loc($bin_name, APACHE_PATH);
        return $bin if $bin;
    }
    return undef;
}

sub _check_apache_module {
    my ($module, $config_name, $mod_info, $output) = @_;
    my $ok;
    if ($mod_info =~ /^\s+\Q$config_name\E\b/m) {
        $ok = 1;
    }
    if ($output) {
        _checking_for({ package => $module, ok => $ok });
    }
    return $ok;
}

sub print_module_instructions {
    my ($check_results, $output) = @_;

    # First we print the long explanatory messages.

    if (scalar @{$check_results->{missing}}) {
        print install_string('modules_message_required');
    }

    if (!$check_results->{one_dbd}) {
        print install_string('modules_message_db');
    }

    if (my @missing = @{$check_results->{optional}} and $output) {
        print install_string('modules_message_optional');
        # Now we have to determine how large the table cols will be.
        my $longest_name = max(map(length($_->{package}), @missing));

        # The first column header is at least 11 characters long.
        $longest_name = 11 if $longest_name < 11;

        # The table is TABLE_WIDTH characters long. There are seven mandatory
        # characters (* and space) in the string. So, we have a total
        # of TABLE_WIDTH - 7 characters to work with.
        my $remaining_space = (TABLE_WIDTH - 7) - $longest_name;
        print '*' x TABLE_WIDTH . "\n";
        printf "* \%${longest_name}s * %-${remaining_space}s *\n",
               'MODULE NAME', 'ENABLES FEATURE(S)';
        print '*' x TABLE_WIDTH . "\n";
        foreach my $package (@missing) {
            printf "* \%${longest_name}s * %-${remaining_space}s *\n",
                   $package->{package}, 
                   _translate_feature($package->{feature});
        }
    }

    if (my @missing = @{ $check_results->{apache} }) {
        print install_string('modules_message_apache');
        my $missing_string = join(', ', @missing);
        my $size = TABLE_WIDTH - 7;
        printf "*    \%-${size}s *\n", $missing_string;
        my $spaces = TABLE_WIDTH - 2;
        print "*", (' ' x $spaces), "*\n";
    }

    my $need_module_instructions =  
        ( (!$output and @{$check_results->{missing}})
          or ($output and $check_results->{any_missing}) ) ? 1 : 0;

    if ($need_module_instructions or @{ $check_results->{apache} }) {
        # If any output was required, we want to close the "table"
        print "*" x TABLE_WIDTH . "\n";
    }

    # And now we print the actual installation commands.

    if (my @missing = @{$check_results->{optional}} and $output) {
        print install_string('commands_optional') . "\n\n";
        foreach my $module (@missing) {
            my $command = install_command($module);
            printf "%15s: $command\n", $module->{package};
        }
        print "\n";
    }

    if (!$check_results->{one_dbd}) {
        print install_string('commands_dbd') . "\n";
        my %db_modules = %{DB_MODULE()};
        foreach my $db (keys %db_modules) {
            my $command = install_command($db_modules{$db}->{dbd});
            printf "%10s: \%s\n", $db_modules{$db}->{name}, $command;
        }
        print "\n";
    }

    if (my @missing = @{$check_results->{missing}}) {
        print colored(install_string('commands_required'), COLOR_ERROR), "\n";
        foreach my $package (@missing) {
            my $command = install_command($package);
            print "    $command\n";
        }
    }

    if ($output && $check_results->{any_missing} && !ON_ACTIVESTATE
        && !$check_results->{hide_all}) 
    {
        print install_string('install_all', { perl => $^X });
    }
    if (!$check_results->{pass}) {
        print colored(install_string('installation_failed'), COLOR_ERROR),
              "\n\n";
    }
}

sub _translate_feature {
    my $features = shift;
    my @strings;
    foreach my $feature (@$features) {
        push(@strings, install_string("feature_$feature"));
    }
    return join(', ', @strings);
}

sub check_graphviz {
    my ($output) = @_;

    my $webdotbase = Bugzilla->params->{'webdotbase'};
    return 1 if $webdotbase =~ /^https?:/;

    my $return;
    $return = 1 if -x $webdotbase;

    if ($output) {
        _checking_for({ package => 'GraphViz', ok => $return });
    }

    if (!$return) {
        print install_string('bad_executable', { bin => $webdotbase }), "\n";
    }

    my $webdotdir = bz_locations()->{'webdotdir'};
    # Check .htaccess allows access to generated images
    if (-e "$webdotdir/.htaccess") {
        my $htaccess = new IO::File("$webdotdir/.htaccess", 'r') 
            || die "$webdotdir/.htaccess: " . $!;
        if (!grep(/png/, $htaccess->getlines)) {
            print STDERR install_string('webdot_bad_htaccess',
                                        { dir => $webdotdir }), "\n";
        }
        $htaccess->close;
    }

    return $return;
}

# This was originally clipped from the libnet Makefile.PL, adapted here for
# accurate version checking.
sub have_vers {
    my ($params, $output) = @_;
    my $module  = $params->{module};
    my $package = $params->{package};
    if (!$package) {
        $package = $module;
        $package =~ s/::/-/g;
    }
    my $wanted  = $params->{version};

    eval "require $module;";
    # Don't let loading a module change the output-encoding of STDOUT
    # or STDERR. (CGI.pm tries to set "binmode" on these file handles when
    # it's loaded, and other modules may do the same in the future.)
    Bugzilla::Install::Util::set_output_encoding();

    # VERSION is provided by UNIVERSAL::, and can be called even if
    # the module isn't loaded. We eval'uate ->VERSION because it can die
    # when the version is not valid (yes, this happens from time to time).
    # In that case, we use an uglier method to get the version.
    my $vnum = eval { $module->VERSION };
    if ($@) {
        no strict 'refs';
        $vnum = ${"${module}::VERSION"};

        # If we come here, then the version is not a valid one.
        # We try to sanitize it.
        if ($vnum =~ /^((\d+)(\.\d+)*)/) {
            $vnum = $1;
        }
    }
    $vnum ||= -1;

    # Must do a string comparison as $vnum may be of the form 5.10.1.
    my $vok = ($vnum ne '-1' && version->new($vnum) >= version->new($wanted)) ? 1 : 0;
    my $blacklisted;
    if ($vok && $params->{blacklist}) {
        $blacklisted = grep($vnum =~ /$_/, @{$params->{blacklist}});
        $vok = 0 if $blacklisted;
    }

    if ($output) {
        _checking_for({ 
            package => $package, ok => $vok, wanted => $wanted,
            found   => $vnum, blacklisted => $blacklisted
        });
    }
    
    return $vok ? 1 : 0;
}

sub _checking_for {
    my ($params) = @_;
    my ($package, $ok, $wanted, $blacklisted, $found) = 
        @$params{qw(package ok wanted blacklisted found)};

    my $ok_string = $ok ? install_string('module_ok') : '';

    # If we're actually checking versions (like for Perl modules), then
    # we have some rather complex logic to determine what we want to 
    # show. If we're not checking versions (like for GraphViz) we just
    # show "ok" or "not found".
    if (exists $params->{found}) {
        my $found_string;
        # We do a string compare in case it's non-numeric. We make sure
        # it's not a version object as negative versions are forbidden.
        if ($found && !ref($found) && $found eq '-1') {
            $found_string = install_string('module_not_found');
        }
        elsif ($found) {
            $found_string = install_string('module_found', { ver => $found });
        }
        else {
            $found_string = install_string('module_unknown_version');
        }
        $ok_string = $ok ? "$ok_string: $found_string" : $found_string;
    }
    elsif (!$ok) {
        $ok_string = install_string('module_not_found');
    }

    my $black_string = $blacklisted ? install_string('blacklisted') : '';
    my $want_string  = $wanted ? "v$wanted" : install_string('any');

    my $str = sprintf "%s %20s %-11s $ok_string $black_string\n",
                install_string('checking_for'), $package, "($want_string)";
    print $ok ? $str : colored($str, COLOR_ERROR);
}

sub install_command {
    my $module = shift;
    my ($command, $package);

    if (ON_ACTIVESTATE) {
        $command = 'ppm install %s';
        $package = $module->{package};
    }
    else {
        $command = "$^X install-module.pl \%s";
        # Non-Windows installations need to use module names, because
        # CPAN doesn't understand package names.
        $package = $module->{module};
    }
    return sprintf $command, $package;
}

# This does a reverse mapping for FEATURE_FILES.
sub map_files_to_features {
    my %features = FEATURE_FILES;
    my %files;
    foreach my $feature (keys %features) {
        my @my_files = @{ $features{$feature} };
        foreach my $pattern (@my_files) {
            foreach my $file (glob $pattern) {
                $files{$file} = $feature;
            }
        }
    }
    return \%files;
}

1;

__END__

=head1 NAME

Bugzilla::Install::Requirements - Functions and variables dealing
  with Bugzilla's perl-module requirements.

=head1 DESCRIPTION

This module is used primarily by C<checksetup.pl> to determine whether
or not all of Bugzilla's prerequisites are installed. (That is, all the
perl modules it requires.)

=head1 CONSTANTS

=over

=item C<REQUIRED_MODULES>

An arrayref of hashrefs that describes the perl modules required by 
Bugzilla. The hashes have three keys: 

=over

=item C<package> - The name of the Perl package that you'd find on
CPAN for this requirement. 

=item C<module> - The name of a module that can be passed to the
C<install> command in C<CPAN.pm> to install this module.

=item C<version> - The version of this module that we require, or C<0>
if any version is acceptable.

=back

=item C<OPTIONAL_MODULES>

An arrayref of hashrefs that describes the perl modules that add
additional features to Bugzilla if installed. Its hashes have all
the fields of L</REQUIRED_MODULES>, plus a C<feature> item--an arrayref
of strings that describe what features require this module.

=item C<FEATURE_FILES>

A hashref that describes what files should only be compiled if a certain
feature is enabled. The feature is the key, and the values are arrayrefs
of file names (which are passed to C<glob>, so shell patterns work).

=back


=head1 SUBROUTINES

=over 4

=item C<check_requirements>

=over

=item B<Description>

This checks what optional or required perl modules are installed, like
C<checksetup.pl> does.

=item B<Params>

=over

=item C<$output> - C<true> if you want the function to print out information
about what it's doing, and the versions of everything installed.

=back

=item B<Returns>

A hashref containing these values:

=over

=item C<pass> - Whether or not we have all the mandatory requirements.

=item C<missing> - An arrayref containing any required modules that
are not installed or that are not up-to-date. Each item in the array is
a hashref in the format of items from L</REQUIRED_MODULES>.

=item C<optional> - The same as C<missing>, but for optional modules.

=item C<apache> - The name of each optional Apache module that is missing.

=item C<have_one_dbd> - True if at least one C<DBD::> module is installed.

=item C<any_missing> - True if there are any missing Perl modules, even
optional modules.

=back

=back

=item C<check_graphviz($output)>

Description: Checks if the graphviz binary specified in the 
  C<webdotbase> parameter is a valid binary, or a valid URL.

Params:      C<$output> - C<$true> if you want the function to
                 print out information about what it's doing.

Returns:     C<1> if the check was successful, C<0> otherwise.

=item C<have_vers($module, $output)>

 Description: Tells you whether or not you have the appropriate
              version of the module requested. It also prints
              out a message to the user explaining the check
              and the result.

 Params:      C<$module> - A hashref, in the format of an item from 
                           L</REQUIRED_MODULES>.
              C<$output> - Set to true if you want this function to
                           print information to STDOUT about what it's
                           doing.

 Returns:   C<1> if you have the module installed and you have the
            appropriate version. C<0> otherwise.

=item C<install_command($module)>

 Description: Prints out the appropriate command to install the
              module specified, depending on whether you're
              on Windows or Linux.

 Params:      C<$module> - A hashref, in the format of an item from
                           L</REQUIRED_MODULES>.

 Returns:     nothing

=item C<map_files_to_features>

Returns a hashref where file names are the keys and the value is the feature
that must be enabled in order to compile that file.

=back

=head1 B<Methods in need of POD>

=over

=item print_module_instructions

=back
