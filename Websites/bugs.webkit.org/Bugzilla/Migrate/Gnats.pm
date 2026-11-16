# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Migrate::Gnats;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Migrate);

use Bugzilla::Constants;
use Bugzilla::Install::Util qw(indicate_progress);
use Bugzilla::Util qw(format_time trim generate_random_password);

use Email::Address;
use Email::MIME;
use File::Basename;
use IO::File;
use List::MoreUtils qw(firstidx);
use List::Util qw(first);

use constant REQUIRED_MODULES => [
    {
        package => 'Email-Simple-FromHandle',
        module  => 'Email::Simple::FromHandle',
        # This version added seekable handles.
        version => 0.050,
    },
];

use constant FIELD_MAP => {
    'Number'         => 'bug_id',
    'Category'       => 'product',
    'Synopsis'       => 'short_desc',
    'Responsible'    => 'assigned_to',
    'State'          => 'bug_status',
    'Class'          => 'cf_type',
    'Classification' => '',
    'Originator'     => 'reporter',
    'Arrival-Date'   => 'creation_ts',
    'Last-Modified'  => 'delta_ts',
    'Release'        => 'version',
    'Severity'       => 'bug_severity',
    'Description'    => 'comment',
};

use constant VALUE_MAP => {
    bug_severity => {
        'serious'      => 'major',
        'cosmetic'     => 'trivial',
        'new-feature'  => 'enhancement',
        'non-critical' => 'normal',
    },
    bug_status => {
        'open'      => 'CONFIRMED',
        'analyzed'  => 'IN_PROGRESS',
        'suspended' => 'RESOLVED',
        'feedback'  => 'RESOLVED',
        'released'  => 'VERIFIED',
    },
    bug_status_resolution => {
        'feedback'  => 'FIXED',
        'released'  => 'FIXED',
        'closed'    => 'FIXED',
        'suspended' => 'LATER',
    },
    priority => {
        'medium' => 'Normal',
    },
};

use constant GNATS_CONFIG_VARS => (
    {
        name    => 'gnats_path',
        default => '/var/lib/gnats',
        desc    => <<END,
# The path to the directory that contains the GNATS database.
END
    },
    {
        name    => 'default_email_domain',
        default => 'example.com',
        desc    => <<'END',
# Some GNATS users do not have full email addresses, but Bugzilla requires
# every user to have an email address. What domain should be appended to
# usernames that don't have emails, to make them into email addresses?
# (For example, if you leave this at the default, "unknown" would become
# "unknown@example.com".)
END
    },
    {
        name    => 'component_name',
        default => 'General',
        desc    => <<'END',
# GNATS has only "Category" to classify bugs. However, Bugzilla has a
# multi-level system of Products that contain Components. When importing
# GNATS categories, they become a Product with one Component. What should
# the name of that Component be?
END
    },
    {
        name    => 'version_regex',
        default => '',
        desc    => <<'END',
# In GNATS, the "version" field can contain almost anything. However, in
# Bugzilla, it's a drop-down, so you don't want too many choices in there.
# If you specify a regular expression here, versions will be tested against
# this regular expression, and if they match, the first match (the first set
# of parentheses in the regular expression, also called "$1") will be used
# as the version value for the bug instead of the full version value specified
# in GNATS.
END
    },
    {
        name    => 'default_originator',
        default => 'gnats-admin',
        desc    => <<'END',
# Sometimes, a PR has no valid Originator, so we fall back to the From
# header of the email. If the From header also isn't a valid username
# (is just a name with spaces in it--we can't convert that to an email
# address) then this username (which can either be a GNATS username or an
# email address) will be considered to be the Originator of the PR.
END
    }
);

sub CONFIG_VARS {
    my $self = shift;
    my @vars = (GNATS_CONFIG_VARS, $self->SUPER::CONFIG_VARS);
    my $field_map = first { $_->{name} eq 'translate_fields' } @vars;
    $field_map->{default} = FIELD_MAP;
    my $value_map = first { $_->{name} eq 'translate_values' } @vars;
    $value_map->{default} = VALUE_MAP;
    return @vars;
}

# Directories that aren't projects, or that we shouldn't be parsing
use constant SKIP_DIRECTORIES => qw(
    gnats-adm
    gnats-queue
    pending
);

use constant NON_COMMENT_FIELDS => qw(
    Audit-Trail
    Closed-Date
    Confidential
    Unformatted
    attachments
);

# Certain fields can contain things that look like fields in them,
# because they might contain quoted emails. To avoid mis-parsing,
# we list out here the exact order of fields at the end of a PR
# and wait for the next field to consider that we actually have
# a field to parse.
use constant END_FIELD_ORDER => qw(
    Description
    How-To-Repeat
    Fix
    Release-Note
    Audit-Trail
    Unformatted
);

use constant CUSTOM_FIELDS => {
    cf_type => {
        type        => FIELD_TYPE_SINGLE_SELECT,
        description => 'Type',
    },
};

use constant FIELD_REGEX => qr/^>(\S+):\s*(.*)$/;

# Used for bugs that have no Synopsis.
use constant NO_SUBJECT => "(no subject)";

# This is the divider that GNATS uses between attachments in its database
# files. It's missign two hyphens at the beginning because MIME Emails use
# -- to start boundaries.
use constant GNATS_BOUNDARY => '----gnatsweb-attachment----';

use constant LONG_VERSION_LENGTH => 32;

#########
# Hooks #
#########

sub before_insert {
    my $self = shift;

    # gnats_id isn't a valid User::create field, and we don't need it
    # anymore now.
    delete $_->{gnats_id} foreach @{ $self->users };

    # Grab a version out of a bug for each product, so that there is a
    # valid "version" argument for Bugzilla::Product->create.
    foreach my $product (@{ $self->products }) {
        my $bug = first { $_->{product} eq $product->{name} and $_->{version} }
                        @{ $self->bugs };
        if (defined $bug) {
            $product->{version} = $bug->{version};
        }
        else {
            $product->{version} = 'unspecified';
        }
    }
}

#########
# Users #
#########

sub _read_users {
    my $self = shift;
    my $path = $self->config('gnats_path');
    my $file =  "$path/gnats-adm/responsible";
    $self->debug("Reading users from $file");
    my $default_domain = $self->config('default_email_domain');
    open(my $users_fh, '<', $file) || die "$file: $!";
    my @users;
    foreach my $line (<$users_fh>) {
        $line = trim($line);
        next if $line =~ /^#/;
        my ($id, $name, $email) = split(':', $line, 3);
        $email ||= "$id\@$default_domain";
        # We can't call our own translate_value, because that depends on
        # the existence of user_map, which doesn't exist until after
        # this method. However, we still want to translate any users found.
        $email = $self->SUPER::translate_value('user', $email);
        push(@users, { realname => $name, login_name => $email,
                       gnats_id => $id });
    }
    close($users_fh);
    return \@users;
}

sub user_map {
    my $self = shift;
    $self->{user_map} ||= { map { $_->{gnats_id} => $_->{login_name} }
                                @{ $self->users } };
    return $self->{user_map};
}

sub add_user {
    my ($self, $id, $email) = @_;
    return if defined $self->user_map->{$id};
    $self->user_map->{$id} = $email;
    push(@{ $self->users }, { login_name => $email, gnats_id => $id });
}

sub user_to_email {
    my ($self, $value) = @_;
    if (defined $self->user_map->{$value}) {
        $value = $self->user_map->{$value};
    }
    elsif ($value !~ /@/) {
        my $domain = $self->config('default_email_domain');
        $value = "$value\@$domain";
    }
    return $value;
}

############
# Products #
############

sub _read_products {
    my $self = shift;
    my $path = $self->config('gnats_path');
    my $file =  "$path/gnats-adm/categories";
    $self->debug("Reading categories from $file");

    open(my $categories_fh, '<', $file) || die "$file: $!";    
    my @products;
    foreach my $line (<$categories_fh>) {
        $line = trim($line);
        next if $line =~ /^#/;
        my ($name, $description, $assigned_to, $cc) = split(':', $line, 4);
        my %product = ( name => $name, description => $description );
        
        my @initial_cc = split(',', $cc);
        @initial_cc = @{ $self->translate_value('user', \@initial_cc) };
        $assigned_to = $self->translate_value('user', $assigned_to);
        my %component = ( name         => $self->config('component_name'),
                          description  => $description,
                          initialowner => $assigned_to,
                          initial_cc   => \@initial_cc );
        $product{components} = [\%component];
        push(@products, \%product);
    }
    close($categories_fh);
    return \@products;
}

################
# Reading Bugs #
################

sub _read_bugs {
    my $self = shift;
    my $path = $self->config('gnats_path');
    my @directories = glob("$path/*");
    my @bugs;
    foreach my $directory (@directories) {
        next if !-d $directory;
        my $name = basename($directory);
        next if grep($_ eq $name, SKIP_DIRECTORIES);
        push(@bugs, @{ $self->_parse_project($directory) });
    }
    @bugs = sort { $a->{Number} <=> $b->{Number} } @bugs;
    return \@bugs;
}

sub _parse_project {
    my ($self, $directory) = @_;
    my @files = glob("$directory/*");

    $self->debug("Reading Project: $directory");
    # Sometimes other files get into gnats directories.
    @files = grep { basename($_) =~ /^\d+$/ } @files;
    my @bugs;
    my $count = 1;
    my $total = scalar @files;
    print basename($directory) . ":\n";
    foreach my $file (@files) {
        push(@bugs, $self->_parse_bug_file($file));
        if (!$self->verbose) {
            indicate_progress({ current => $count++, every => 5,
                                total => $total });
        }
    }
    return \@bugs;
}

sub _parse_bug_file {
    my ($self, $file) = @_;
    $self->debug("Reading $file");
    open(my $fh, "<", $file) || die "$file: $!";
    my $email = Email::Simple::FromHandle->new($fh);
    my $fields = $self->_get_gnats_field_data($email);
    # We parse attachments here instead of during translate_bug,
    # because otherwise we'd be taking up huge amounts of memory storing
    # all the raw attachment data in memory.
    $fields->{attachments} = $self->_parse_attachments($fields);
    close($fh);
    return $fields;
}

sub _get_gnats_field_data {
    my ($self, $email) = @_;
    my ($current_field, @value_lines, %fields);
    $email->reset_handle();
    my $handle = $email->handle;
    foreach my $line (<$handle>) {
        # If this line starts a field name
        if ($line =~ FIELD_REGEX) {
            my ($new_field, $rest_of_line) = ($1, $2);
            
            # If this is one of the last few PR fields, then make sure
            # that we're getting our fields in the right order.
            my $new_field_valid = 1;
            my $search_for = $current_field || '';
            my $current_field_pos = firstidx { $_ eq $search_for }
                                             END_FIELD_ORDER;
            if ($current_field_pos > -1) {
                my $new_field_pos = firstidx { $_ eq $new_field } 
                                             END_FIELD_ORDER;
                # We accept any field, as long as it's later than this one.
                $new_field_valid = $new_field_pos > $current_field_pos ? 1 : 0;
            }
            
            if ($new_field_valid) {
                if ($current_field) {
                    $fields{$current_field} = _handle_lines(\@value_lines);
                    @value_lines = ();
                }
                $current_field = $new_field;
                $line = $rest_of_line;
            }
        }
        push(@value_lines, $line) if defined $line;
    }
    $fields{$current_field} = _handle_lines(\@value_lines);
    $fields{cc} = [$email->header('Cc')] if $email->header('Cc');
    
    # If the Originator is invalid and we don't have a translation for it,
    # use the From header instead.
    my $originator = $self->translate_value('reporter', $fields{Originator},
                                            { check_only => 1 });
    if ($originator !~ Bugzilla->params->{emailregexp}) {
        # We use the raw header sometimes, because it looks like "From: user"
        # which Email::Address won't parse but we can still use.
        my $address = $email->header('From');
        my ($parsed) = Email::Address->parse($address);
        if ($parsed) {
            $address = $parsed->address;
        }
        if ($address) {
            $self->debug(
                "PR $fields{Number} had an Originator that was not a valid"
                . " user ($fields{Originator}). Using From ($address)"
                . " instead.\n");
            my $address_email = $self->translate_value('reporter', $address,
                                                       { check_only => 1 });
            if ($address_email !~ Bugzilla->params->{emailregexp}) {
                $self->debug(" From was also invalid, using default_originator.\n");
                $address = $self->config('default_originator');
            }
            $fields{Originator} = $address;
        }
    }

    $self->debug(\%fields, 3);
    return \%fields;
}

sub _handle_lines {
    my ($lines) = @_;
    my $value = join('', @$lines);
    $value =~ s/\s+$//;
    return $value;
}

####################
# Translating Bugs #
####################

sub translate_bug {
    my ($self, $fields) = @_;

    my ($bug, $other_fields) = $self->SUPER::translate_bug($fields);

    $bug->{attachments} = delete $other_fields->{attachments};

    if (defined $other_fields->{_add_to_comment}) {
        $bug->{comment} .= delete $other_fields->{_add_to_comment};
    }

    my ($changes, $extra_comment) =
        $self->_parse_audit_trail($bug, $other_fields->{'Audit-Trail'});

    my @comments;
    foreach my $change (@$changes) {
        if (exists $change->{comment}) {
            push(@comments, {
                thetext  => $change->{comment},
                who      => $change->{who},
                bug_when => $change->{bug_when} });
            delete $change->{comment};
        }
    }
    $bug->{history}  = $changes;

    if (trim($extra_comment)) {
        push(@comments, { thetext => $extra_comment, who => $bug->{reporter},
                          bug_when => $bug->{delta_ts} || $bug->{creation_ts} });
    }
    $bug->{comments} = \@comments;
    
    $bug->{component} = $self->config('component_name');
    if (!$bug->{short_desc}) {
        $bug->{short_desc} = NO_SUBJECT;
    }
    
    foreach my $attachment (@{ $bug->{attachments} || [] }) {
        $attachment->{submitter} = $bug->{reporter};
        $attachment->{creation_ts} = $bug->{creation_ts};
    }

    $self->debug($bug, 3);
    return $bug;
}

sub _parse_audit_trail {
    my ($self, $bug, $audit_trail) = @_;
    return [] if !trim($audit_trail);
    $self->debug(" Parsing audit trail...", 2);
    
    if ($audit_trail !~ /^\S+-Changed-\S+:/ms) {
        # This is just a comment from the bug's creator.
        $self->debug("  Audit trail is just a comment.", 2);
        return ([], $audit_trail);
    }
    
    my (@changes, %current_data, $current_column, $on_why);
    my $extra_comment = '';
    my $current_field;
    my @all_lines = split("\n", $audit_trail);
    foreach my $line (@all_lines) {
        # GNATS history looks like:
        # Status-Changed-From-To: open->closed
        # Status-Changed-By: jack
        # Status-Changed-When: Mon May 12 14:46:59 2003
        # Status-Changed-Why:
        #     This is some comment here about the change.
        if ($line =~ /^(\S+)-Changed-(\S+):(.*)/) {
            my ($field, $column, $value) = ($1, $2, $3);
            my $bz_field = $self->translate_field($field);
            # If it's not a field we're importing, we don't care about
            # its history.
            next if !$bz_field;
            # GNATS doesn't track values for description changes,
            # unfortunately, and that's the only information we'd be able to
            # use in Bugzilla for the audit trail on that field.
            next if $bz_field eq 'comment';
            $current_field = $bz_field if !$current_field;
            if ($bz_field ne $current_field) {
                $self->_store_audit_change(
                    \@changes, $current_field, \%current_data);
                %current_data = ();
                $current_field = $bz_field;
            }
            $value = trim($value);
            $self->debug("  $bz_field $column: $value", 3);
            if ($column eq 'From-To') {
                my ($from, $to) = split('->', $value, 2);
                # Sometimes there's just a - instead of a -> between the values.
                if (!defined($to)) {
                    ($from, $to) = split('-', $value, 2);
                }
                $current_data{added} = $to;
                $current_data{removed} = $from;
            }
            elsif ($column eq 'By') {
                my $email = $self->translate_value('user', $value);
                # Sometimes we hit users in the audit trail that we haven't
                # seen anywhere else.
                $current_data{who} = $email;
            }
            elsif ($column eq 'When') {
                $current_data{bug_when} = $self->parse_date($value);
            }
            if ($column eq 'Why') {
                $value = '' if !defined $value;
                $current_data{comment} = $value;
                $on_why = 1;
            }
            else {
                $on_why = 0;
            }
        }
        elsif ($on_why) {
            # "Why" lines are indented four characters.
            $line =~ s/^\s{4}//;
            $current_data{comment} .= "$line\n";
        }
        else {
            $self->debug(
                "Extra Audit-Trail line on $bug->{product} $bug->{bug_id}:"
                 . " $line\n", 2);
            $extra_comment .= "$line\n";
        }
    }
    $self->_store_audit_change(\@changes, $current_field, \%current_data);
    return (\@changes, $extra_comment);
}

sub _store_audit_change {
    my ($self, $changes, $old_field, $current_data) = @_;

    $current_data->{field} = $old_field;
    $current_data->{removed} = 
        $self->translate_value($old_field, $current_data->{removed});
    $current_data->{added} =
        $self->translate_value($old_field, $current_data->{added});
    push(@$changes, { %$current_data });
}

sub _parse_attachments {
    my ($self, $fields) = @_;
    my $unformatted = delete $fields->{'Unformatted'};
    my $gnats_boundary = GNATS_BOUNDARY;
    # A sanity checker to make sure that we're parsing attachments right.
    my $num_attachments = 0;
    $num_attachments++ while ($unformatted =~ /\Q$gnats_boundary\E/g);
    # Sometimes there's a GNATS_BOUNDARY that is on the same line as other data.
    $unformatted =~ s/(\S\s*)\Q$gnats_boundary\E$/$1\n$gnats_boundary/mg;
    # Often the "Unformatted" section starts with stuff before
    # ----gnatsweb-attachment---- that isn't necessary.
    $unformatted =~ s/^\s*From:.+?Reply-to:[^\n]+//s;
    $unformatted = trim($unformatted);
    return [] if !$unformatted;
    $self->debug('Reading attachments...', 2);
    my $boundary = generate_random_password(48);
    $unformatted =~ s/\Q$gnats_boundary\E/--$boundary/g;
    # Sometimes the whole Unformatted section is indented by exactly
    # one space, and needs to be fixed.
    if ($unformatted =~ /--\Q$boundary\E\n /) {
        $unformatted =~ s/^ //mg;
    }
    $unformatted = <<END;
From: nobody
MIME-Version: 1.0
Content-Type: multipart/mixed; boundary="$boundary"

This is a multi-part message in MIME format.
--$boundary
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: 7bit

$unformatted
--$boundary--
END
    my $email = new Email::MIME(\$unformatted);
    my @parts = $email->parts;
    # Remove the fake body.
    my $part1 = shift @parts;
    if ($part1->body) {
        $self->debug(" Additional Unformatted data found on "
                     . $fields->{Category} . " bug " . $fields->{Number});
        $self->debug($part1->body, 3);
        $fields->{_add_comment} .= "\n\nUnformatted:\n" . $part1->body;
    }

    my @attachments;
    foreach my $part (@parts) {
        $self->debug('  Parsing attachment: ' . $part->filename);
        my $temp_fh = IO::File->new_tmpfile or die ("Can't create tempfile: $!");
        $temp_fh->binmode;
        print $temp_fh $part->body;
        my $content_type = $part->content_type;
        $content_type =~ s/; name=.+$//;
        my $attachment = { filename    => $part->filename,
                           description => $part->filename,
                           mimetype    => $content_type,
                           data        => $temp_fh };
        $self->debug($attachment, 3);
        push(@attachments, $attachment);
    }
    
    if (scalar(@attachments) ne $num_attachments) {
        warn "WARNING: Expected $num_attachments attachments but got "
             . scalar(@attachments) . "\n" ;
        $self->debug($unformatted, 3);
    }
    return \@attachments;
}

sub translate_value {
    my $self = shift;
    my ($field, $value, $options) = @_;
    my $original_value = $value;
    $options ||= {};

    if (!ref($value) and grep($_ eq $field, $self->USER_FIELDS)) {
        if ($value =~ /(\S+\@\S+)/) {
            $value = $1;
            $value =~ s/^<//;
            $value =~ s/>$//;
        }
        else {
            # Sometimes names have extra stuff on the end like "(Somebody's Name)"
            $value =~ s/\s+\(.+\)$//;
            # Sometimes user fields look like "(user)" instead of just "user".
            $value =~ s/^\((.+)\)$/$1/;
            $value = trim($value);
        }
    }

    if ($field eq 'version' and $value ne '') {
        my $version_re = $self->config('version_regex');
        if ($version_re and $value =~ $version_re) {
            $value = $1;
        }
        # In the GNATS that I tested this with, there were many extremely long
        # values for "version" that caused some import problems (they were
        # longer than the max allowed version value). So if the version value
        # is longer than 32 characters, pull out the first thing that looks
        # like a version number.
        elsif (length($value) > LONG_VERSION_LENGTH) {
            $value =~ s/^.+?\b(\d[\w\.]+)\b.+$/$1/;
        }
    }
    
    my @args = @_;
    $args[1] = $value;
    
    $value = $self->SUPER::translate_value(@args);
    return $value if ref $value;
    
    if (grep($_ eq $field, $self->USER_FIELDS)) {
        my $from_value = $value;
        $value = $self->user_to_email($value);
        $args[1] = $value;
        # If we got something new from user_to_email, do any necessary
        # translation of it.
        $value = $self->SUPER::translate_value(@args);
        if (!$options->{check_only}) {
            $self->add_user($from_value, $value);
        }
    }
    
    return $value;
}

1;

=head1 B<Methods in need of POD>

=over

=item user_map

=item user_to_email

=item add_user

=item translate_value

=item before_insert

=item translate_bug

=item CONFIG_VARS

=back
