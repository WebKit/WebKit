# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Update;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Constants;

use constant TIME_INTERVAL => 86400; # Default is one day, in seconds.
use constant TIMEOUT       => 5; # Number of seconds before timeout.

# Look for new releases and notify logged in administrators about them.
sub get_notifications {
    return if !Bugzilla->feature('updates');
    return if (Bugzilla->params->{'upgrade_notification'} eq 'disabled');

    my $local_file = bz_locations()->{'datadir'} . '/' . LOCAL_FILE;
    # Update the local XML file if this one doesn't exist or if
    # the last modification time (stat[9]) is older than TIME_INTERVAL.
    if (!-e $local_file || (time() - (stat($local_file))[9] > TIME_INTERVAL)) {
        unlink $local_file; # Make sure the old copy is away.
        return { 'error' => 'no_update' } if (-e $local_file);

        my $error = _synchronize_data();
        # If an error is returned, leave now.
        return $error if $error;
    }

    # If we cannot access the local XML file, ignore it.
    return { 'error' => 'no_access' } unless (-r $local_file);

    my $twig = XML::Twig->new();
    $twig->safe_parsefile($local_file);
    # If the XML file is invalid, return.
    return { 'error' => 'corrupted' } if $@;
    my $root = $twig->root;

    my @releases;
    foreach my $branch ($root->children('branch')) {
        my $release = {
            'branch_ver' => $branch->{'att'}->{'id'},
            'latest_ver' => $branch->{'att'}->{'vid'},
            'status'     => $branch->{'att'}->{'status'},
            'url'        => $branch->{'att'}->{'url'},
            'date'       => $branch->{'att'}->{'date'}
        };
        push(@releases, $release);
    }

    # On which branch is the current installation running?
    my @current_version =
        (BUGZILLA_VERSION =~ m/^(\d+)\.(\d+)(?:(rc|\.)(\d+))?\+?$/);

    my @release;
    if (Bugzilla->params->{'upgrade_notification'} eq 'development_snapshot') {
        @release = grep {$_->{'status'} eq 'development'} @releases;
        # If there is no development snapshot available, then we are in the
        # process of releasing a release candidate. That's the release we want.
        unless (scalar(@release)) {
            @release = grep {$_->{'status'} eq 'release-candidate'} @releases;
        }
    }
    elsif (Bugzilla->params->{'upgrade_notification'} eq 'latest_stable_release') {
        @release = grep {$_->{'status'} eq 'stable'} @releases;
    }
    elsif (Bugzilla->params->{'upgrade_notification'} eq 'stable_branch_release') {
        # We want the latest stable version for the current branch.
        # If we are running a development snapshot, we won't match anything.
        my $branch_version = $current_version[0] . '.' . $current_version[1];

        # We do a string comparison instead of a numerical one, because
        # e.g. 2.2 == 2.20, but 2.2 ne 2.20 (and 2.2 is indeed much older).
        @release = grep {$_->{'branch_ver'} eq $branch_version} @releases;

        # If the branch is now closed, we should strongly suggest
        # to upgrade to the latest stable release available.
        if (scalar(@release) && $release[0]->{'status'} eq 'closed') {
            @release = grep {$_->{'status'} eq 'stable'} @releases;
            return {'data' => $release[0], 'deprecated' => $branch_version};
        }
    }
    else {
      # Unknown parameter.
      return {'error' => 'unknown_parameter'};
    }

    # Return if no new release is available.
    return unless scalar(@release);

    # Only notify the administrator if the latest version available
    # is newer than the current one.
    my @new_version =
        ($release[0]->{'latest_ver'} =~ m/^(\d+)\.(\d+)(?:(rc|\.)(\d+))?\+?$/);

    # We convert release candidates 'rc' to integers (rc ? 0 : 1) in order
    # to compare versions easily.
    $current_version[2] = ($current_version[2] && $current_version[2] eq 'rc') ? 0 : 1;
    $new_version[2] = ($new_version[2] && $new_version[2] eq 'rc') ? 0 : 1;

    my $is_newer = _compare_versions(\@current_version, \@new_version);
    return ($is_newer == 1) ? {'data' => $release[0]} : undef;
}

sub _synchronize_data {
    my $local_file = bz_locations()->{'datadir'} . '/' . LOCAL_FILE;

    my $ua = LWP::UserAgent->new();
    $ua->timeout(TIMEOUT);
    $ua->protocols_allowed(['http', 'https']);
    # If the URL of the proxy is given, use it, else get this information
    # from the environment variable.
    my $proxy_url = Bugzilla->params->{'proxy_url'};
    if ($proxy_url) {
        $ua->proxy(['http', 'https'], $proxy_url);
    }
    else {
        $ua->env_proxy;
    }
    my $response = eval { $ua->mirror(REMOTE_FILE, $local_file) };

    # $ua->mirror() forces the modification time of the local XML file
    # to match the modification time of the remote one.
    # So we have to update it manually to reflect that a newer version
    # of the file has effectively been requested. This will avoid
    # any new download for the next TIME_INTERVAL.
    if (-e $local_file) {
        # Try to alter its last modification time.
        my $can_alter = utime(undef, undef, $local_file);
        # This error should never happen.
        $can_alter || return { 'error' => 'no_update' };
    }
    elsif ($response && $response->is_error) {
        # We have been unable to download the file.
        return { 'error' => 'cannot_download', 'reason' => $response->status_line };
    }
    else {
        return { 'error' => 'no_write', 'reason' => $@ };
    }

    # Everything went well.
    return 0;
}

sub _compare_versions {
    my ($old_ver, $new_ver) = @_;
    while (scalar(@$old_ver) && scalar(@$new_ver)) {
        my $old = shift(@$old_ver) || 0;
        my $new = shift(@$new_ver) || 0;
        return $new <=> $old if ($new <=> $old);
    }
    return scalar(@$new_ver) <=> scalar(@$old_ver);

}

1;

__END__

=head1 NAME

Bugzilla::Update - Update routines for Bugzilla

=head1 SYNOPSIS

  use Bugzilla::Update;

  # Get information about new releases
  my $new_release = Bugzilla::Update::get_notifications();

=head1 DESCRIPTION

This module contains all required routines to notify you
about new releases. It downloads an XML file from bugzilla.org
and parses it, in order to display information based on your
preferences. Absolutely no information about the Bugzilla version
you are running is sent to bugzilla.org.

=head1 FUNCTIONS

=over

=item C<get_notifications()>

 Description: This function informs you about new releases, if any.

 Params:      None.

 Returns:     On success, a reference to a hash with data about
              new releases, if any.
              On failure, a reference to a hash with the reason
              of the failure and the name of the unusable XML file.

=back

=cut
