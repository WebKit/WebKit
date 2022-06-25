# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


package Bugzilla::User::Setting;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);


# Module stuff
@Bugzilla::User::Setting::EXPORT = qw(
    get_all_settings
    get_defaults
    add_setting
    clear_settings_cache
);

use Bugzilla::Error;
use Bugzilla::Util qw(trick_taint get_text);

###############################
###  Module Initialization  ###
###############################

sub new {
    my $invocant = shift;
    my $setting_name = shift;
    my $user_id = shift;

    my $class = ref($invocant) || $invocant;
    my $subclass = '';

    # Create a ref to an empty hash and bless it
    my $self = {};

    my $dbh = Bugzilla->dbh;

    # Confirm that the $setting_name is properly formed;
    # if not, throw a code error. 
    # 
    # NOTE: due to the way that setting names are used in templates,
    # they must conform to to the limitations set for HTML NAMEs and IDs.
    #
    if ( !($setting_name =~ /^[a-zA-Z][-.:\w]*$/) ) {
      ThrowCodeError("setting_name_invalid", { name => $setting_name });
    }

    # If there were only two parameters passed in, then we need
    # to retrieve the information for this setting ourselves.
    if (scalar @_ == 0) {

        my ($default, $is_enabled, $value);
        ($default, $is_enabled, $value, $subclass) = 
          $dbh->selectrow_array(
             q{SELECT default_value, is_enabled, setting_value, subclass
                 FROM setting
            LEFT JOIN profile_setting
                   ON setting.name = profile_setting.setting_name
                WHERE name = ?
                  AND profile_setting.user_id = ?},
             undef, 
             $setting_name, $user_id);

        # if not defined, then grab the default value
        if (! defined $value) {
            ($default, $is_enabled, $subclass) =
              $dbh->selectrow_array(
                 q{SELECT default_value, is_enabled, subclass
                   FROM setting
                   WHERE name = ?},
              undef,
              $setting_name);
        }

        $self->{'is_enabled'} = $is_enabled;
        $self->{'default_value'} = $default;

        # IF the setting is enabled, AND the user has chosen a setting
        # THEN return that value
        # ELSE return the site default, and note that it is the default.
        if ( ($is_enabled) && (defined $value) ) {
            $self->{'value'} = $value;
        } else {
            $self->{'value'} = $default;
            $self->{'isdefault'} = 1;
        }
    }
    else {
        # If the values were passed in, simply assign them and return.
        $self->{'is_enabled'}    = shift;
        $self->{'default_value'} = shift;
        $self->{'value'}         = shift;
        $self->{'is_default'}    = shift;
        $subclass                = shift;
    }
    if ($subclass) {
        eval('require ' . $class . '::' . $subclass);
        $@ && ThrowCodeError('setting_subclass_invalid',
                             {'subclass' => $subclass});
        $class = $class . '::' . $subclass;
    }
    bless($self, $class);

    $self->{'_setting_name'} = $setting_name;
    $self->{'_user_id'}      = $user_id;

    return $self;
}

###############################
###  Subroutine Definitions ###
###############################

sub add_setting {
    my ($name, $values, $default_value, $subclass, $force_check,
        $silently) = @_;
    my $dbh = Bugzilla->dbh;

    my $exists = _setting_exists($name);
    return if ($exists && !$force_check);

    ($name && length( $default_value // '' ))
      ||  ThrowCodeError("setting_info_invalid");

    if ($exists) {
        # If this setting exists, we delete it and regenerate it.
        $dbh->do('DELETE FROM setting_value WHERE name = ?', undef, $name);
        $dbh->do('DELETE FROM setting WHERE name = ?', undef, $name);
        # Remove obsolete user preferences for this setting.
        if (defined $values && scalar(@$values)) {
            my $list = join(', ', map {$dbh->quote($_)} @$values);
            $dbh->do("DELETE FROM profile_setting
                      WHERE setting_name = ? AND setting_value NOT IN ($list)",
                      undef, $name);
        }
    }
    elsif (!$silently) {
        print get_text('install_setting_new', { name => $name }) . "\n";
    }
    $dbh->do(q{INSERT INTO setting (name, default_value, is_enabled, subclass)
                    VALUES (?, ?, 1, ?)},
             undef, ($name, $default_value, $subclass));

    my $sth = $dbh->prepare(q{INSERT INTO setting_value (name, value, sortindex)
                                    VALUES (?, ?, ?)});

    my $sortindex = 5;
    foreach my $key (@$values){
        $sth->execute($name, $key, $sortindex);
        $sortindex += 5;
    }
}

sub get_all_settings {
    my ($user_id) = @_;
    my $settings = {};
    my $dbh = Bugzilla->dbh;

    my $cache_key = "user_settings.$user_id";
    my $rows = Bugzilla->memcached->get_config({ key => $cache_key });
    if (!$rows) {
        $rows = $dbh->selectall_arrayref(
            q{SELECT name, default_value, is_enabled, setting_value, subclass
                FROM setting
           LEFT JOIN profile_setting
                     ON setting.name = profile_setting.setting_name
                     AND profile_setting.user_id = ?}, undef, ($user_id));
        Bugzilla->memcached->set_config({ key => $cache_key, data => $rows });
    }

    foreach my $row (@$rows) {
        my ($name, $default_value, $is_enabled, $value, $subclass) = @$row;

        my $is_default;

        if ( ($is_enabled) && (defined $value) ) {
            $is_default = 0;
        } else {
            $value = $default_value;
            $is_default = 1;
        }

        $settings->{$name} = new Bugzilla::User::Setting(
           $name, $user_id, $is_enabled,
           $default_value, $value, $is_default, $subclass);
    }

    return $settings;
}

sub clear_settings_cache {
    my ($user_id) = @_;
    Bugzilla->memcached->clear_config({ key => "user_settings.$user_id" });
}

sub get_defaults {
    my ($user_id) = @_;
    my $dbh = Bugzilla->dbh;
    my $default_settings = {};

    $user_id ||= 0;

    my $rows = $dbh->selectall_arrayref(q{SELECT name, default_value, is_enabled, subclass
                                            FROM setting});

    foreach my $row (@$rows) {
        my ($name, $default_value, $is_enabled, $subclass) = @$row;

        $default_settings->{$name} = new Bugzilla::User::Setting(
            $name, $user_id, $is_enabled, $default_value, $default_value, 1,
            $subclass);
    }

    return $default_settings;
}

sub set_default {
    my ($setting_name, $default_value, $is_enabled) = @_;
    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare(q{UPDATE setting
                                 SET default_value = ?, is_enabled = ?
                               WHERE name = ?});
    $sth->execute($default_value, $is_enabled, $setting_name);
}

sub _setting_exists {
    my ($setting_name) = @_;
    my $dbh = Bugzilla->dbh;
    return $dbh->selectrow_arrayref(
        "SELECT 1 FROM setting WHERE name = ?", undef, $setting_name) || 0;
}


sub legal_values {
    my ($self) = @_;

    return $self->{'legal_values'} if defined $self->{'legal_values'};

    my $dbh = Bugzilla->dbh;
    $self->{'legal_values'} = $dbh->selectcol_arrayref(
              q{SELECT value
                  FROM setting_value
                 WHERE name = ?
              ORDER BY sortindex},
        undef, $self->{'_setting_name'});

    return $self->{'legal_values'};
}

sub validate_value {
    my $self = shift;

    if (grep(/^$_[0]$/, @{$self->legal_values()})) {
        trick_taint($_[0]);
    }
    else {
        ThrowCodeError('setting_value_invalid',
                       {'name'  => $self->{'_setting_name'},
                        'value' => $_[0]});
    }
}

sub reset_to_default {
    my ($self) = @_;

    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->do(q{ DELETE
                            FROM profile_setting
                           WHERE setting_name = ?
                             AND user_id = ?},
                       undef, $self->{'_setting_name'}, $self->{'_user_id'});
      $self->{'value'}       = $self->{'default_value'};
      $self->{'is_default'}  = 1;
}

sub set {
    my ($self, $value) = @_;
    my $dbh = Bugzilla->dbh;
    my $query;

    if ($self->{'is_default'}) {
        $query = q{INSERT INTO profile_setting
                   (setting_value, setting_name, user_id)
                   VALUES (?,?,?)};
    } else {
        $query = q{UPDATE profile_setting
                      SET setting_value = ?
                    WHERE setting_name = ?
                      AND user_id = ?};
    }
    $dbh->do($query, undef, $value, $self->{'_setting_name'}, $self->{'_user_id'});

    $self->{'value'}       = $value;
    $self->{'is_default'}  = 0;
}



1;

__END__

=head1 NAME

Bugzilla::User::Setting - Object for a user preference setting

=head1 SYNOPSIS

Setting.pm creates a setting object, which is a hash containing the user
preference information for a single preference for a single user. These 
are usually accessed through the "settings" object of a user, and not 
directly.

=head1 DESCRIPTION

use Bugzilla::User::Setting;
my $settings;

$settings->{$setting_name} = new Bugzilla::User::Setting(
   $setting_name, $user_id);

OR

$settings->{$setting_name} = new Bugzilla::User::Setting(
   $setting_name, $user_id, $is_enabled,
   $default_value, $value, $is_default);

=head1 CLASS FUNCTIONS

=over 4

=item C<add_setting($name, \@values, $default_value, $subclass, $force_check)>

Description: Checks for the existence of a setting, and adds it 
             to the database if it does not yet exist.

Params:      C<$name> - string - the name of the new setting
             C<$values> - arrayref - contains the new choices
               for the new Setting.
             C<$default_value> - string - the site default
             C<$subclass> - string - name of the module returning
               the list of valid values. This means legal values are
               not stored in the DB.
             C<$force_check> - boolean - when true, the existing setting
               and all its values are deleted and replaced by new data.

Returns:     a pointer to a hash of settings


=item C<get_all_settings($user_id)>

Description: Provides the user's choices for each setting in the 
             system; if the user has made no choice, uses the site
             default instead.
Params:      C<$user_id> - integer - the user id.
Returns:     a pointer to a hash of settings

=item C<get_defaults($user_id)>

Description: When a user is not logged in, they must use the site
             defaults for every settings; this subroutine provides them.
Params:      C<$user_id> (optional) - integer - the user id.  Note that
             this optional parameter is mainly for internal use only.
Returns:     A pointer to a hash of settings.  If $user_id was passed, set
             the user_id value for each setting.

=item C<set_default($setting_name, $default_value, $is_enabled)>

Description: Sets the global default for a given setting. Also sets
             whether users are allowed to choose their own value for
             this setting, or if they must use the global default.
Params:      C<$setting_name> - string - the name of the setting
             C<$default_value> - string - the new default value for this setting
             C<$is_enabled> - boolean - if false, all users must use the global default
Returns:     nothing

=item C<clear_settings_cache($user_id)>

Description: Clears cached settings data for the specified user.  Must be
             called after updating any user's setting.
Params:      C<$user_id> - integer - the user id.
Returns:     nothing

=begin private

=item C<_setting_exists>

Description: Determines if a given setting exists in the database.
Params:      C<$setting_name> - string - the setting name
Returns:     boolean - true if the setting already exists in the DB.

=end private

=back

=head1 METHODS

=over 4

=item C<legal_values($setting_name)>

Description: Returns all legal values for this setting
Params:      none
Returns:     A reference to an array containing all legal values

=item C<validate_value>

Description: Determines whether a value is valid for the setting
             by checking against the list of legal values.
             Untaints the parameter if the value is indeed valid,
             and throws a setting_value_invalid code error if not.
Params:      An lvalue containing a candidate for a setting value
Returns:     nothing

=item C<reset_to_default>

Description: If a user chooses to use the global default for a given 
             setting, their saved entry is removed from the database via 
             this subroutine.
Params:      none
Returns:     nothing

=item C<set($value)>

Description: If a user chooses to use their own value rather than the 
             global value for a given setting, OR changes their value for
             a given setting, this subroutine is called to insert or 
             update the database as appropriate.
Params:      C<$value> - string - the new value for this setting for this user.
Returns:     nothing

=back
