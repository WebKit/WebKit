# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Config;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);
use autodie qw(:default);

use Bugzilla::Constants;
use Bugzilla::Hook;
use Bugzilla::Util qw(trick_taint read_text write_text);

use JSON::XS;
use File::Temp;
use File::Basename;

# Don't export localvars by default - people should have to explicitly
# ask for it, as a (probably futile) attempt to stop code using it
# when it shouldn't
%Bugzilla::Config::EXPORT_TAGS =
  (
   admin => [qw(update_params SetParam write_params)],
  );
Exporter::export_ok_tags('admin');

# INITIALISATION CODE
# Perl throws a warning if we use bz_locations() directly after do.
our %params;
# Load in the param definitions
sub _load_params {
    my $panels = param_panels();
    my %hook_panels;
    foreach my $panel (keys %$panels) {
        my $module = $panels->{$panel};
        eval("require $module") || die $@;
        my @new_param_list = $module->get_param_list();
        $hook_panels{lc($panel)} = { params => \@new_param_list };
    }
    # This hook is also called in editparams.cgi. This call here is required
    # to make SetParam work.
    Bugzilla::Hook::process('config_modify_panels', 
                            { panels => \%hook_panels });

    foreach my $panel (keys %hook_panels) {
        foreach my $item (@{$hook_panels{$panel}->{params}}) {
            $params{$item->{'name'}} = $item;
        }
    }
}
# END INIT CODE

# Subroutines go here

sub param_panels {
    my $param_panels = {};
    my $libpath = bz_locations()->{'libpath'};
    foreach my $item ((glob "$libpath/Bugzilla/Config/*.pm")) {
        $item =~ m#/([^/]+)\.pm$#;
        my $module = $1;
        $param_panels->{$module} = "Bugzilla::Config::$module" unless $module eq 'Common';
    }
    # Now check for any hooked params
    Bugzilla::Hook::process('config_add_panels', 
                            { panel_modules => $param_panels });
    return $param_panels;
}

sub SetParam {
    my ($name, $value) = @_;

    _load_params unless %params;
    die "Unknown param $name" unless (exists $params{$name});

    my $entry = $params{$name};

    # sanity check the value

    # XXX - This runs the checks. Which would be good, except that
    # check_shadowdb creates the database as a side effect, and so the
    # checker fails the second time around...
    if ($name ne 'shadowdb' && exists $entry->{'checker'}) {
        my $err = $entry->{'checker'}->($value, $entry);
        die "Param $name is not valid: $err" unless $err eq '';
    }

    Bugzilla->params->{$name} = $value;
}

sub update_params {
    my ($params) = @_;
    my $answer = Bugzilla->installation_answers;
    my $datadir = bz_locations()->{'datadir'};
    my $param;

    # If the old data/params file using Data::Dumper output still exists,
    # read it. It will be deleted once the parameters are stored in the new
    # data/params.json file.
    my $old_file = "$datadir/params";

    if (-e $old_file) {
        require Safe;
        my $s = new Safe;

        $s->rdo($old_file);
        die "Error reading $old_file: $!" if $!;
        die "Error evaluating $old_file: $@" if $@;

        # Now read the param back out from the sandbox.
        $param = \%{ $s->varglob('param') };
    }
    else {
        # Rename params.js to params.json if checksetup.pl
        # was executed with an earlier version of this change
        rename "$old_file.js", "$old_file.json"
            if -e "$old_file.js" && !-e "$old_file.json";

        # Read the new data/params.json file.
        $param = read_param_file();
    }

    my %new_params;

    # If we didn't return any param values, then this is a new installation.
    my $new_install = !(keys %$param);

    # --- UPDATE OLD PARAMS ---

    # Change from usebrowserinfo to defaultplatform/defaultopsys combo
    if (exists $param->{'usebrowserinfo'}) {
        if (!$param->{'usebrowserinfo'}) {
            if (!exists $param->{'defaultplatform'}) {
                $new_params{'defaultplatform'} = 'Other';
            }
            if (!exists $param->{'defaultopsys'}) {
                $new_params{'defaultopsys'} = 'Other';
            }
        }
    }

    # Change from a boolean for quips to multi-state
    if (exists $param->{'usequip'} && !exists $param->{'enablequips'}) {
        $new_params{'enablequips'} = $param->{'usequip'} ? 'on' : 'off';
    }

    # Change from old product groups to controls for group_control_map
    # 2002-10-14 bug 147275 bugreport@peshkin.net
    if (exists $param->{'usebuggroups'} && 
        !exists $param->{'makeproductgroups'}) 
    {
        $new_params{'makeproductgroups'} = $param->{'usebuggroups'};
    }

    # Modularise auth code
    if (exists $param->{'useLDAP'} && !exists $param->{'loginmethod'}) {
        $new_params{'loginmethod'} = $param->{'useLDAP'} ? "LDAP" : "DB";
    }

    # set verify method to whatever loginmethod was
    if (exists $param->{'loginmethod'} 
        && !exists $param->{'user_verify_class'}) 
    {
        $new_params{'user_verify_class'} = $param->{'loginmethod'};
    }

    # Remove quip-display control from parameters
    # and give it to users via User Settings (Bug 41972)
    if ( exists $param->{'enablequips'} 
         && !exists $param->{'quip_list_entry_control'}) 
    {
        my $new_value;
        ($param->{'enablequips'} eq 'on')       && do {$new_value = 'open';};
        ($param->{'enablequips'} eq 'approved') && do {$new_value = 'moderated';};
        ($param->{'enablequips'} eq 'frozen')   && do {$new_value = 'closed';};
        ($param->{'enablequips'} eq 'off')      && do {$new_value = 'closed';};
        $new_params{'quip_list_entry_control'} = $new_value;
    }

    # Old mail_delivery_method choices contained no uppercase characters
    my $mta = $param->{'mail_delivery_method'};
    if ($mta) {
        if ($mta !~ /[A-Z]/) {
            my %translation = (
                'sendmail' => 'Sendmail',
                'smtp'     => 'SMTP',
                'qmail'    => 'Qmail',
                'testfile' => 'Test',
                'none'     => 'None');
            $param->{'mail_delivery_method'} = $translation{$mta};
        }
        # This will force the parameter to be reset to its default value.
        delete $param->{'mail_delivery_method'} if $param->{'mail_delivery_method'} eq 'Qmail';
    }

    # Convert the old "ssl" parameter to the new "ssl_redirect" parameter.
    # Both "authenticated sessions" and "always" turn on "ssl_redirect"
    # when upgrading.
    if (exists $param->{'ssl'} and $param->{'ssl'} ne 'never') {
        $new_params{'ssl_redirect'} = 1;
    }

    # "specific_search_allow_empty_words" has been renamed to "search_allow_no_criteria".
    if (exists $param->{'specific_search_allow_empty_words'}) {
        $new_params{'search_allow_no_criteria'} = $param->{'specific_search_allow_empty_words'};
    }

    # --- DEFAULTS FOR NEW PARAMS ---

    _load_params unless %params;
    foreach my $name (keys %params) {
        my $item = $params{$name};
        unless (exists $param->{$name}) {
            print "New parameter: $name\n" unless $new_install;
            if (exists $new_params{$name}) {
                $param->{$name} = $new_params{$name};
            }
            elsif (exists $answer->{$name}) {
                $param->{$name} = $answer->{$name};
            }
            else {
                $param->{$name} = $item->{'default'};
            }
        }
    }

    $param->{'utf8'} = 1 if $new_install;

    # Bug 452525: OR based groups are on by default for new installations
    $param->{'or_groups'} = 1 if $new_install;

    # --- REMOVE OLD PARAMS ---

    my %oldparams;
    # Remove any old params
    foreach my $item (keys %$param) {
        if (!exists $params{$item}) {
            $oldparams{$item} = delete $param->{$item};
        }
    }

    # Write any old parameters to old-params.txt
    my $old_param_file = "$datadir/old-params.txt";
    if (scalar(keys %oldparams)) {
        my $op_file = new IO::File($old_param_file, '>>', 0600)
          || die "Couldn't create $old_param_file: $!";

        print "The following parameters are no longer used in Bugzilla,",
              " and so have been\nmoved from your parameters file into",
              " $old_param_file:\n";

        my $comma = "";
        foreach my $item (keys %oldparams) {
            print $op_file "\n\n$item:\n" . $oldparams{$item} . "\n";
            print "${comma}$item";
            $comma = ", ";
        }
        print "\n";
        $op_file->close;
    }

    write_params($param);

    if (-e $old_file) {
        unlink $old_file;
        say "$old_file has been converted into $old_file.json, using the JSON format.";
    }

    # Return deleted params and values so that checksetup.pl has a chance
    # to convert old params to new data.
    return %oldparams;
}

sub write_params {
    my ($param_data) = @_;
    $param_data ||= Bugzilla->params;
    my $param_file = bz_locations()->{'datadir'} . '/params.json';

    my $json_data = JSON::XS->new->canonical->pretty->encode($param_data);
    write_text($param_file, $json_data);

    # It's not common to edit parameters and loading
    # Bugzilla::Install::Filesystem is slow.
    require Bugzilla::Install::Filesystem;
    Bugzilla::Install::Filesystem::fix_file_permissions($param_file);

    # And now we have to reset the params cache so that Bugzilla will re-read
    # them.
    delete Bugzilla->request_cache->{params};
}

sub read_param_file {
    my %params;
    my $file = bz_locations()->{'datadir'} . '/params.json';

    if (-e $file) {
        my $data = read_text($file);
        trick_taint($data);

        # If params.json has been manually edited and e.g. some quotes are
        # missing, we don't want JSON::XS to leak the content of the file
        # to all users in its error message, so we have to eval'uate it.
        %params = eval { %{JSON::XS->new->decode($data)} };
        if ($@) {
            my $error_msg = (basename($0) eq 'checksetup.pl') ?
                $@ : 'run checksetup.pl to see the details.';
            die "Error parsing $file: $error_msg";
        }
        # JSON::XS doesn't detaint data for us.
        foreach my $key (keys %params) {
            if (ref($params{$key}) eq "ARRAY") {
                foreach my $item (@{$params{$key}}) {
                    trick_taint($item);
                }
            } else {
                trick_taint($params{$key}) if defined $params{$key};
            }
        }
    }
    elsif ($ENV{'SERVER_SOFTWARE'}) {
       # We're in a CGI, but the params file doesn't exist. We can't
       # Template Toolkit, or even install_string, since checksetup
       # might not have thrown an error. Bugzilla::CGI->new
       # hasn't even been called yet, so we manually use CGI::Carp here
       # so that the user sees the error.
       require CGI::Carp;
       CGI::Carp->import('fatalsToBrowser');
       die "The $file file does not exist."
           . ' You probably need to run checksetup.pl.',
    }
    return \%params;
}

1;

__END__

=head1 NAME

Bugzilla::Config - Configuration parameters for Bugzilla

=head1 SYNOPSIS

  # Administration functions
  use Bugzilla::Config qw(:admin);

  update_params();
  SetParam($param, $value);
  write_params();

=head1 DESCRIPTION

This package contains ways to access Bugzilla configuration parameters.

=head1 FUNCTIONS

=head2 Parameters

Parameters can be set, retrieved, and updated.

=over 4

=item C<SetParam($name, $value)>

Sets the param named $name to $value. Values are checked using the checker
function for the given param if one exists.

=item C<update_params()>

Updates the parameters, by transitioning old params to new formats, setting
defaults for new params, and removing obsolete ones. Used by F<checksetup.pl>
in the process of an installation or upgrade.

Prints out information about what it's doing, if it makes any changes.

May prompt the user for input, if certain required parameters are not
specified.

=item C<write_params($params)>

Description: Writes the parameters to disk.

Params:      C<$params> (optional) - A hashref to write to the disk
               instead of C<Bugzilla-E<gt>params>. Used only by
               C<update_params>.

Returns:     nothing

=item C<read_param_file()>

Description: Most callers should never need this. This is used
             by C<Bugzilla-E<gt>params> to directly read C<$datadir/params.json>
             and load it into memory. Use C<Bugzilla-E<gt>params> instead.

Params:      none

Returns:     A hashref containing the current params in C<$datadir/params.json>.

=item C<param_panels()>

=back
