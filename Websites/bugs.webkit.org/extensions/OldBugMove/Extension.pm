# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::OldBugMove;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Extension);
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Field::Choice;
use Bugzilla::Mailer;
use Bugzilla::User;
use Bugzilla::Util qw(trim);

use Scalar::Util qw(blessed);
use Storable qw(dclone);

use constant VERSION => BUGZILLA_VERSION;

# This is 4 because that's what it originally was when this code was
# a part of Bugzilla.
use constant CMT_MOVED_TO => 4;

sub install_update_db {
    my $reso_type = Bugzilla::Field::Choice->type('resolution');
    my $moved_reso = $reso_type->new({ name => 'MOVED' });
    # We make the MOVED resolution inactive, so that it doesn't show up
    # as a valid drop-down option.
    if ($moved_reso) {
        $moved_reso->set_is_active(0);
        $moved_reso->update();
    }
    else {
        print "Creating the MOVED resolution...\n";
        $reso_type->create(
           { value   => 'MOVED', sortkey => '30000', isactive => 0 });
    }
}

sub config_add_panels {
    my ($self, $args) = @_;
    my $modules = $args->{'panel_modules'};
    $modules->{'OldBugMove'} = 'Bugzilla::Extension::OldBugMove::Params';
}

sub template_before_create {
    my ($self, $args) = @_;
    my $config = $args->{config};

    my $constants = $config->{CONSTANTS};
    $constants->{CMT_MOVED_TO} = CMT_MOVED_TO;

    my $vars = $config->{VARIABLES};
    $vars->{oldbugmove_user_is_mover} = \&_user_is_mover;
}

sub object_before_delete {
    my ($self, $args) = @_;
    my $object = $args->{'object'};
    if ($object->isa('Bugzilla::Field::Choice::resolution')) {
        if ($object->name eq 'MOVED') {
            ThrowUserError('oldbugmove_no_delete_moved');
        }
    }
}

sub object_before_set {
    my ($self, $args) = @_;
    my ($object, $field) = @$args{qw(object field)};
    if ($field eq 'resolution' and $object->isa('Bugzilla::Bug')) {
        # Store the old value so that end_of_set can check it.
        $object->{'_oldbugmove_old_resolution'} = $object->resolution;
    }
}

sub object_end_of_set {
    my ($self, $args) = @_;
    my ($object, $field) = @$args{qw(object field)};
    if ($field eq 'resolution' and $object->isa('Bugzilla::Bug')) {
        my $old_value = delete $object->{'_oldbugmove_old_resolution'};
        return if $old_value eq $object->resolution;
        if ($object->resolution eq 'MOVED') {
            $object->add_comment('', { type => CMT_MOVED_TO,
                                       extra_data => Bugzilla->user->login });
        }
    }
}

sub object_end_of_set_all {
    my ($self, $args) = @_;
    my $object = $args->{'object'};

    if ($object->isa('Bugzilla::Bug') and Bugzilla->input_params->{'oldbugmove'}) {
        my $new_status = Bugzilla->params->{'duplicate_or_move_bug_status'};
        $object->set_bug_status($new_status, { resolution => 'MOVED' });
    }
}

sub object_validators {
    my ($self, $args) = @_;
    my ($class, $validators) = @$args{qw(class validators)};
    if ($class->isa('Bugzilla::Comment')) {
        my $extra_data_validator = $validators->{extra_data};
        $validators->{extra_data} = 
            sub { _check_comment_extra_data($extra_data_validator, @_) };
    }
    elsif ($class->isa('Bugzilla::Bug')) {
        my $reso_validator = $validators->{resolution};
        $validators->{resolution} =
            sub { _check_bug_resolution($reso_validator, @_) };
    }
}

sub _check_bug_resolution {
    my $original_validator = shift;
    my ($invocant, $resolution) = @_;

    if ($resolution eq 'MOVED' && $invocant->resolution ne 'MOVED'
        && !Bugzilla->input_params->{'oldbugmove'})
    {
        # MOVED has a special meaning and can only be used when
        # really moving bugs to another installation.
        ThrowUserError('oldbugmove_no_manual_move');
    }

    return $original_validator->(@_);
}

sub _check_comment_extra_data {
    my $original_validator = shift;
    my ($invocant, $extra_data, undef, $params) = @_;
    my $type = blessed($invocant) ? $invocant->type : $params->{type};

    if ($type == CMT_MOVED_TO) {
        return Bugzilla::User->check($extra_data)->login;
    }
    return $original_validator->(@_);
}

sub bug_end_of_update {
    my ($self, $args) = @_;
    my ($bug, $old_bug, $changes) = @$args{qw(bug old_bug changes)};
    if (defined $changes->{'resolution'}
        and $changes->{'resolution'}->[1] eq 'MOVED')
    {
        $self->_move_bug($bug, $old_bug);
    }
}

sub _move_bug {
    my ($self, $bug, $old_bug) = @_;

    my $dbh = Bugzilla->dbh;
    my $template = Bugzilla->template;

    _user_is_mover(Bugzilla->user) 
        or ThrowUserError("auth_failure", { action => 'move',
                                            object => 'bugs' });

    # Don't export the new status and resolution. We want the current
    # ones.
    local $Storable::forgive_me = 1;
    my $export_me = dclone($bug);
    $export_me->{bug_status} = $old_bug->bug_status;
    delete $export_me->{status};
    $export_me->{resolution} = $old_bug->resolution;

    # Prepare and send all data about these bugs to the new database
    my $to = Bugzilla->params->{'move-to-address'};
    $to =~ s/@/\@/;
    my $from = Bugzilla->params->{'mailfrom'};
    $from =~ s/@/\@/;
    my $msg = "To: $to\n";
    $msg .= "From: Bugzilla <" . $from . ">\n";
    $msg .= "Subject: Moving bug " . $bug->id . "\n\n";
    my @fieldlist = (Bugzilla::Bug->fields, 'group', 'long_desc',
                     'attachment', 'attachmentdata');
    my %displayfields = map { $_ => 1 } @fieldlist;
    my $vars = { bugs => [$export_me], displayfields => \%displayfields };
    $template->process("bug/show.xml.tmpl", $vars, \$msg)
      || ThrowTemplateError($template->error());
    $msg .= "\n";
    MessageToMTA($msg);
}

sub _user_is_mover {
    my $user = shift;

    my @movers = map { trim($_) } split(',', Bugzilla->params->{'movers'});
    return ($user->id and grep($_ eq $user->login, @movers)) ? 1 : 0;
}

__PACKAGE__->NAME;
