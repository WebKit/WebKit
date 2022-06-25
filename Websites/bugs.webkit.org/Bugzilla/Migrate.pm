# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Migrate;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Attachment;
use Bugzilla::Bug qw(LogActivityEntry);
use Bugzilla::Component;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Install::Requirements ();
use Bugzilla::Install::Util qw(indicate_progress);
use Bugzilla::Product;
use Bugzilla::Util qw(get_text trim generate_random_password);
use Bugzilla::User ();
use Bugzilla::Status ();
use Bugzilla::Version;

use Data::Dumper;
use Date::Parse;
use DateTime;
use Fcntl qw(SEEK_SET);
use File::Basename;
use List::Util qw(first);
use Safe;

use constant CUSTOM_FIELDS      => {};
use constant REQUIRED_MODULES   => [];
use constant NON_COMMENT_FIELDS => ();

use constant CONFIG_VARS => (
    {
        name    => 'translate_fields',
        default => {},
        desc    => <<'END',
# This maps field names in your bug-tracker to Bugzilla field names. If a field
# has the same name in your bug-tracker and Bugzilla (case-insensitively), it
# doesn't need a mapping here. If a field isn't listed here and doesn't have
# an equivalent field in Bugzilla, its data will be added to the initial
# description of each bug migrated. If the right side is an empty string, it
# means "just put the value of this field into the initial description of the
# bug".
#
# Generally, you can keep the defaults, here.
#
# If you want to know the internal names of various Bugzilla fields
# (as used on the right side here), see the fielddefs table in the Bugzilla
# database.
#
# If you are mapping to any custom fields in Bugzilla, you have to create
# the custom fields using Bugzilla Administration interface before you run
# migrate.pl. However, if they are drop down or multi-select fields, you 
# don't have to populate the list of values--migrate.pl will do that for you.
# Some migrators create certain custom fields by default. If you see a
# field name starting with "cf_" on the right side of this configuration
# variable by default, then that field will be automatically created by
# the migrator and you don't have to worry about it.
END
    },
    {
        name    => 'translate_values',
        default => {},
        desc    => <<'END',
# This configuration variable allows you to say that a particular field
# value in your current bug-tracker should be translated to a different
# value when it's imported into Bugzilla.
#
# The value of this variable should look something like this:
#
# {
#     bug_status => {
#         # Translate "Handled" into "RESOLVED".
#         "Handled"     => "RESOLVED",
#         "In Progress" => "IN_PROGRESS",
#     },
#
#     priority => {
#         # Translate "Serious" into "Highest"
#         "Serious" => "Highest",
#     },
# };
#
# Values are translated case-insensitively, so "foo" will match "Foo", "FOO",
# and "foo".
#
# Note that the field names used are *Bugzilla* field names (from the fielddefs
# table in the database), not the field names from your current bug-tracker.
#
# The special field name "user" will be used to translate any field that
# can contain a user, including reporter, assigned_to, qa_contact, and cc.
# You should use "user" instead of specifying reporter, assigned_to, etc.
# manually.
#
# The special field "bug_status_resolution" can be used to give certain
# statuses in your bug-tracker a resolution in Bugzilla. So, for example,
# you could translate the "fixed" status in your Bugzilla to "RESOLVED"
# in the "bug_status" field, and then put "fixed => 'FIXED'" in the
# "bug_status_resolution" field to translated a "fixed" bug into
# RESOLVED FIXED in Bugzilla.
#
# Values that don't get translated will be imported as-is.
END
    },
    {
        name    => 'starting_bug_id',
        default => 0,
        desc    => <<'END',
# What bug ID do you want the first imported bug to get? If you set this to
# 0, then the imported bug ids will just start right after the current
# bug ids. If you use this configuration variable, you must make sure that
# nobody else is using your Bugzilla while you run the migration, or a new
# bug filed by a user might take this ID instead.
END
    },
    {
        name    => 'timezone',
        default => 'local',
        desc => <<'END',
# If migrate.pl comes across any dates without timezones, while doing the
# migration, what timezone should we assume those dates are in? 
# The best format for this variable is something like "America/Los Angeles".
# However, time zone abbreviations (like PST, PDT, etc.) are also acceptable,
# but will result in a less-accurate conversion of times and dates.
#
# The special value "local" means "use the same timezone as the system I
# am running this script on now".
END
    },
);

use constant USER_FIELDS => qw(user assigned_to qa_contact reporter cc);

#########################
# Main Migration Method #
#########################

sub do_migration {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    # On MySQL, setting serial values implicitly commits a transaction,
    # so we want to do it up here, outside of any transaction. This also
    # has the advantage of loading the config before anything else is done.
    if ($self->config('starting_bug_id')) {
        $dbh->bz_set_next_serial_value('bugs', 'bug_id',
                                       $self->config('starting_bug_id'));
    }    
    $dbh->bz_start_transaction();

    $self->before_read();
    # Read Other Database
    my $users    = $self->users;
    my $products = $self->products;
    my $bugs     = $self->bugs;
    $self->after_read();
    
    $self->translate_all_bugs($bugs);

    Bugzilla->set_user(Bugzilla::User->super_user);
    
    # Insert into Bugzilla
    $self->before_insert();
    $self->insert_users($users);
    $self->insert_products($products);
    $self->create_custom_fields();
    $self->create_legal_values($bugs);
    $self->insert_bugs($bugs);
    $self->after_insert();
    if ($self->dry_run) {
        $dbh->bz_rollback_transaction();
        $self->reset_serial_values();
    }
    else {
        $dbh->bz_commit_transaction();
    }
}

################
# Constructors #
################

sub new {
    my ($class) = @_;
    my $self = { };
    bless $self, $class;
    return $self;
}

sub load {
    my ($class, $from) = @_;
    my $libdir = bz_locations()->{libpath};
    my @migration_modules = glob("$libdir/Bugzilla/Migrate/*");
    my ($module) = grep { basename($_) =~ /^\Q$from\E\.pm$/i }
                          @migration_modules;
    if (!$module) {
        ThrowUserError('migrate_from_invalid', { from => $from });
    }
    require $module;
    my $canonical_name = _canonical_name($module);
    return "Bugzilla::Migrate::$canonical_name"->new;
}

#############
# Accessors #
#############

sub name {
    my $self = shift;
    return _canonical_name(ref $self);
}

sub dry_run {
    my ($self, $value) = @_;
    if (scalar(@_) > 1) {
        $self->{dry_run} = $value;
    }
    return $self->{dry_run} || 0;
}


sub verbose {
    my ($self, $value) = @_;
    if (scalar(@_) > 1) {
        $self->{verbose} = $value;
    }
    return $self->{verbose} || 0;
}

sub debug {
    my ($self, $value, $level) = @_;
    $level ||= 1;
    if ($self->verbose >= $level) {
        $value = Dumper($value) if ref $value;
        print STDERR $value, "\n";
    }
}

sub bug_fields {
    my $self = shift;
    $self->{bug_fields} ||= Bugzilla->fields({ by_name => 1 });
    return $self->{bug_fields};
}

sub users {
    my $self = shift;
    if (!exists $self->{users}) {
        say get_text('migrate_reading_users');
        $self->{users} = $self->_read_users();
    }
    return $self->{users};
}

sub products {
    my $self = shift;
    if (!exists $self->{products}) {
        say get_text('migrate_reading_products');
        $self->{products} = $self->_read_products();
    }
    return $self->{products};
}

sub bugs {
    my $self = shift;
    if (!exists $self->{bugs}) {
        say get_text('migrate_reading_bugs');
        $self->{bugs} = $self->_read_bugs();
    }
    return $self->{bugs};
}

###########
# Methods #
###########

sub check_requirements {
    my $self = shift;
    my $missing = Bugzilla::Install::Requirements::_check_missing(
        $self->REQUIRED_MODULES, 1);
    my %results = (
        apache      => [],
        pass        => @$missing ? 0 : 1,
        missing     => $missing,
        any_missing => @$missing ? 1 : 0,
        hide_all    => 1,
        # These are just for compatibility with print_module_instructions
        one_dbd  => 1,
        optional => [],
    );
    Bugzilla::Install::Requirements::print_module_instructions(
        \%results, 1);
    exit(1) if @$missing;
}

sub reset_serial_values {
    my $self = shift;
    return if $self->{serial_values_reset};
    my $dbh = Bugzilla->dbh;
    my %reset = (
        'bugs'        => 'bug_id',
        'attachments' => 'attach_id',
        'profiles'    => 'userid',
        'longdescs'   => 'comment_id',
        'products'    => 'id',
        'components'  => 'id',
        'versions'    => 'id',
        'milestones'  => 'id',
    );
    my @select_fields = grep { $_->is_select } (values %{ $self->bug_fields });
    foreach my $field (@select_fields) {
        next if $field->is_abnormal;
        $reset{$field->name} = 'id';
    }
    
    while (my ($table, $column) = each %reset) {
        $dbh->bz_set_next_serial_value($table, $column);
    }
    
    $self->{serial_values_reset} = 1;
}

###################
# Bug Translation #
###################

sub translate_all_bugs {
    my ($self, $bugs) = @_;
    say get_text('migrate_translating_bugs');
    # We modify the array in place so that $self->bugs will return the
    # modified bugs, in case $self->before_insert wants them.
    my $num_bugs = scalar(@$bugs);
    for (my $i = 0; $i < $num_bugs; $i++) {
        $bugs->[$i] = $self->translate_bug($bugs->[$i]);
    }
}

sub translate_bug {
    my ($self, $fields) = @_;
    my (%bug, %other_fields);
    my $original_status;
    foreach my $field (keys %$fields) {
        my $value = delete $fields->{$field};
        my $bz_field = $self->translate_field($field);
        if ($bz_field) {
            $bug{$bz_field} = $self->translate_value($bz_field, $value);
            if ($bz_field eq 'bug_status') {
                $original_status = $value;
            }
        }
        else {
            $other_fields{$field} = $value;
        }
    }
    
    if (defined $original_status and !defined $bug{resolution}
        and $self->map_value('bug_status_resolution', $original_status))
    {
        $bug{resolution} = $self->map_value('bug_status_resolution',
                                            $original_status);
    }
    
    $bug{comment} = $self->_generate_description(\%bug, \%other_fields);
    
    return wantarray ? (\%bug, \%other_fields) : \%bug;
}

sub _generate_description {
    my ($self, $bug, $fields) = @_;
    
    my $description = "";
    foreach my $field (sort keys %$fields) {
        next if grep($_ eq $field, $self->NON_COMMENT_FIELDS);
        my $value = delete $fields->{$field};
        next if $value eq '';
        $description .= "$field: $value\n";
    }
    $description .= "\n" if $description;

    return $description . $bug->{comment};
}

sub translate_field {
    my ($self, $field) = @_;
    my $mapped = $self->config('translate_fields')->{$field};
    return $mapped if defined $mapped;
    ($mapped) = grep { lc($_) eq lc($field) } (keys %{ $self->bug_fields });
    return $mapped;
}

sub parse_date {
    my ($self, $date) = @_;
    my @time = strptime($date);
    # Handle times with timezones that strptime doesn't know about.
    if (!scalar @time) {
        $date =~ s/\s+\S+$//;
        @time = strptime($date);
    }
    my $tz;
    if ($time[6]) {
        $tz = DateTime::TimeZone->offset_as_string($time[6]);
    }
    else {
        $tz = $self->config('timezone');
        $tz =~ s/\s/_/g;
        if ($tz eq 'local') {
            $tz = Bugzilla->local_timezone;
        }
    }
    my $dt = DateTime->new({
        year   => $time[5] + 1900,
        month  => $time[4] + 1,
        day    => $time[3],
        hour   => $time[2],
        minute => $time[1],
        second => int($time[0]),
        time_zone => $tz, 
    });
    $dt->set_time_zone(Bugzilla->local_timezone);
    return $dt->iso8601;
}

sub translate_value {
    my ($self, $field, $value) = @_;
    
    if (!defined $value) {
        warn("Got undefined value for $field\n");
        $value = '';
    }
    
    if (ref($value) eq 'ARRAY') {
        return [ map($self->translate_value($field, $_), @$value) ];
    }

    
    if (defined $self->map_value($field, $value)) {
        return $self->map_value($field, $value);
    }
    
    if (grep($_ eq $field, USER_FIELDS)) {
        if (defined $self->map_value('user', $value)) {
            return $self->map_value('user', $value);
        }
    }

    my $field_obj = $self->bug_fields->{$field};
    if ($field eq 'creation_ts'
        or $field eq 'delta_ts'
        or ($field_obj and
             ($field_obj->type == FIELD_TYPE_DATETIME
              or $field_obj->type == FIELD_TYPE_DATE)))
    {
        $value = trim($value);
        return undef if !$value;
        return $self->parse_date($value);
    }
    
    return $value;
}


sub map_value {
    my ($self, $field, $value) = @_;
    return $self->_value_map->{$field}->{lc($value)};
}

sub _value_map {
    my $self = shift;
    if (!defined $self->{_value_map}) {
        # Lowercase all values to make them case-insensitive.
        my %map;
        my $translation = $self->config('translate_values');
        foreach my $field (keys %$translation) {
            my $value_mapping = $translation->{$field};
            foreach my $value (keys %$value_mapping) {
                $map{$field}->{lc($value)} = $value_mapping->{$value};
            }
        }
        $self->{_value_map} = \%map;
    }
    return $self->{_value_map};
}

#################
# Configuration #
#################

sub config {
    my ($self, $var) = @_;
    if (!exists $self->{config}) {
        $self->{config} = $self->read_config;
    }
    return $self->{config}->{$var};
}

sub config_file_name {
    my $self = shift;
    my $name = $self->name;
    my $dir = bz_locations()->{datadir};
    return "$dir/migrate-$name.cfg"
}

sub read_config {
    my ($self) = @_;
    my $file = $self->config_file_name;
    if (!-e $file) {
        $self->write_config();
        ThrowUserError('migrate_config_created', { file => $file });
    }
    open(my $fh, "<", $file) || die "$file: $!";
    my $safe = new Safe;
    $safe->rdo($file);
    my @read_symbols = map($_->{name}, $self->CONFIG_VARS);
    my %config;
    foreach my $var (@read_symbols) {
        my $glob = $safe->varglob($var);
        $config{$var} = $$glob;
    }
    return \%config;
}

sub write_config {
    my ($self) = @_;
    my $file = $self->config_file_name;
    open(my $fh, ">", $file) || die "$file: $!";
    # Fixed indentation
    local $Data::Dumper::Indent = 1;
    local $Data::Dumper::Quotekeys = 0;
    local $Data::Dumper::Sortkeys = 1;
    foreach my $var ($self->CONFIG_VARS) {
        print $fh "\n", $var->{desc},
        Data::Dumper->Dump([$var->{default}], [$var->{name}]);
    }
    close($fh);
}

####################################
# Default Implementations of Hooks #
####################################

sub after_insert  {}
sub before_insert {}
sub after_read    {}
sub before_read   {}

#############
# Inserters #
#############

sub insert_users {
    my ($self, $users) = @_;
    foreach my $user (@$users) {
        next if new Bugzilla::User({ name => $user->{login_name} });
        my $generated_password;
        if (!defined $user->{cryptpassword}) {
            $generated_password = lc(generate_random_password());
            $user->{cryptpassword} = $generated_password;
        }
        my $created = Bugzilla::User->create($user);
        print get_text('migrate_user_created',
                       { created  => $created,
                         password => $generated_password }), "\n";
    }
}

# XXX This should also insert Classifications.
sub insert_products {
    my ($self, $products) = @_;
    foreach my $product (@$products) {
        my $components = delete $product->{components};
        
        my $created_prod = new Bugzilla::Product({ name => $product->{name} });
        if (!$created_prod) {
            $created_prod = Bugzilla::Product->create($product);
            print get_text('migrate_product_created',
                           { created => $created_prod }), "\n";
        }
        
        foreach my $component (@$components) {
            next if new Bugzilla::Component({ product => $created_prod,
                                              name    => $component->{name} });
            my $created_comp = Bugzilla::Component->create(
                { %$component, product => $created_prod });
            print '  ', get_text('migrate_component_created',
                                 { comp => $created_comp,
                                   product => $created_prod }), "\n";
        }
    }
}

sub create_custom_fields {
    my $self = shift;
    foreach my $field (keys %{ $self->CUSTOM_FIELDS }) {
        next if new Bugzilla::Field({ name => $field });
        my %values = %{ $self->CUSTOM_FIELDS->{$field} };
        # We set these all here for the dry-run case.
        my $created = { %values, name => $field, custom => 1 };
        if (!$self->dry_run) {
            $created = Bugzilla::Field->create($created);
        }
        say get_text('migrate_field_created', { field => $created });
    }
    delete $self->{bug_fields};
}

sub create_legal_values {
    my ($self, $bugs) = @_;
    my @select_fields = grep($_->is_select, values %{ $self->bug_fields });

    # Get all the values in use on all the bugs we're importing.
    my (%values, %product_values);
    foreach my $bug (@$bugs) {
        foreach my $field (@select_fields) {
            my $name = $field->name;
            next if !defined $bug->{$name};
            $values{$name}->{$bug->{$name}} = 1;
        }
        foreach my $field (qw(version target_milestone)) {
            # Fix per-product bug values here, because it's easier than
            # doing it during _insert_bugs.
            if (!defined $bug->{$field} or trim($bug->{$field}) eq '') {
                my $accessor = $field;
                $accessor =~ s/^target_//; $accessor .= "s";
                my $product = Bugzilla::Product->check($bug->{product});
                $bug->{$field} = $product->$accessor->[0]->name;
                next;
            }
            $product_values{$bug->{product}}->{$field}->{$bug->{$field}} = 1;
        }
    }
    
    foreach my $field (@select_fields) {
        next if $field->is_abnormal;
        my $name = $field->name;
        foreach my $value (keys %{ $values{$name} }) {
            next if Bugzilla::Field::Choice->type($field)->new({ name => $value });
            Bugzilla::Field::Choice->type($field)->create({ value => $value });
            print get_text('migrate_value_created',
                           { field => $field, value => $value }), "\n";
        }
    }
    
    foreach my $product (keys %product_values) {
        my $prod_obj = Bugzilla::Product->check($product);
        foreach my $version (keys %{ $product_values{$product}->{version} }) {
            next if new Bugzilla::Version({ product => $prod_obj,
                                            name    => $version });
            my $created = Bugzilla::Version->create({ product => $prod_obj,
                                                      value   => $version });
            my $field = $self->bug_fields->{version};
            print get_text('migrate_value_created', { product => $prod_obj,
                                                      field   => $field,
                                                      value   => $created->name }), "\n";
        }
        foreach my $milestone (keys %{ $product_values{$product}->{target_milestone} }) {
            next if new Bugzilla::Milestone({ product => $prod_obj,
                                              name    => $milestone });
            my $created = Bugzilla::Milestone->create(
                { product => $prod_obj, value => $milestone });
            my $field = $self->bug_fields->{target_milestone};
            print get_text('migrate_value_created', { product => $prod_obj,
                                                      field   => $field,
                                                      value   => $created->name }), "\n";
            
        }
    }
    
}

sub insert_bugs {
    my ($self, $bugs) = @_;
    my $dbh = Bugzilla->dbh;
    say get_text('migrate_creating_bugs');

    my $init_statuses = Bugzilla::Status->can_change_to();
    my %allowed_statuses = map { lc($_->name) => 1 } @$init_statuses;
    # Bypass the question of whether or not we can file UNCONFIRMED
    # in any product by simply picking a non-UNCONFIRMED status as our
    # default for bugs that don't have a status specified.
    my $default_status = first { $_->name ne 'UNCONFIRMED' } @$init_statuses;
    # Use the first resolution that's not blank.
    my $default_resolution =
        first { $_->name ne '' }
              @{ $self->bug_fields->{resolution}->legal_values };

    # Set the values of any required drop-down fields that aren't set.
    my @standard_drop_downs = grep { !$_->custom and $_->is_select }
                                   (values %{ $self->bug_fields });
    # Make bug_status get set before resolution.
    @standard_drop_downs = sort { $a->name cmp $b->name } @standard_drop_downs;
    # Cache all statuses for setting the resolution.
    my %statuses = map { lc($_->name) => $_ } Bugzilla::Status->get_all;

    my $total = scalar @$bugs;
    my $count = 1;
    foreach my $bug (@$bugs) {
        my $comments    = delete $bug->{comments};
        my $history     = delete $bug->{history};
        my $attachments = delete $bug->{attachments};

        $self->debug($bug, 3);

        foreach my $field (@standard_drop_downs) {
            next if $field->is_abnormal;
            my $field_name = $field->name;
            if (!defined $bug->{$field_name}) {
                # If there's a default value for this, then just let create()
                # pick it.
                next if grep($_->is_default, @{ $field->legal_values });
                # Otherwise, pick the first valid value if this is a required
                # field.
                if ($field_name eq 'bug_status') {
                    $bug->{bug_status} = $default_status;
                }
                elsif ($field_name eq 'resolution') {
                    my $status = $statuses{lc($bug->{bug_status})};
                    if (!$status->is_open) {
                        $bug->{resolution} =  $default_resolution;
                    }
                }
                else {
                    $bug->{$field_name} = $field->legal_values->[0]->name;
                }
            }
        }
        
        my $product = Bugzilla::Product->check($bug->{product});
        
        # If this isn't a legal starting status, or if the bug has a
        # resolution, then those will have to be set after creating the bug.
        # We make them into objects so that we can normalize their names.
        my ($set_status, $set_resolution);
        if (defined $bug->{resolution}) {
            $set_resolution = Bugzilla::Field::Choice->type('resolution')
                              ->new({ name => delete $bug->{resolution} });
        }
        if (!$allowed_statuses{lc($bug->{bug_status})}) {
            $set_status = new Bugzilla::Status({ name => $bug->{bug_status} });
            # Set the starting status to some status that Bugzilla will
            # accept. We're going to overwrite it immediately afterward.
            $bug->{bug_status} = $default_status;
        }
        
        # If we're in dry-run mode, our custom fields haven't been created
        # yet, so we shouldn't try to set them on creation.
        if ($self->dry_run) {
            foreach my $field (keys %{ $self->CUSTOM_FIELDS }) {
                delete $bug->{$field};
            }
        }
        
        # File the bug as the reporter.
        my $super_user = Bugzilla->user;
        my $reporter = Bugzilla::User->check($bug->{reporter});
        # Allow the user to file a bug in any product, no matter their current
        # permissions.
        $reporter->{groups} = $super_user->groups;
        Bugzilla->set_user($reporter);
        my $created = Bugzilla::Bug->create($bug);
        $self->debug('Created bug ' . $created->id);
        Bugzilla->set_user($super_user);

        if (defined $bug->{creation_ts}) {
            $dbh->do('UPDATE bugs SET creation_ts = ?, delta_ts = ? 
                       WHERE bug_id = ?', undef, $bug->{creation_ts},
                     $bug->{creation_ts}, $created->id);
        }
        if (defined $bug->{delta_ts}) {
            $dbh->do('UPDATE bugs SET delta_ts = ? WHERE bug_id = ?',
                     undef, $bug->{delta_ts}, $created->id);
        }
        # We don't need to send email for imported bugs.
        $dbh->do('UPDATE bugs SET lastdiffed = delta_ts WHERE bug_id = ?',
                 undef, $created->id);

        # We don't use set_ and update() because that would create
        # a bugs_activity entry that we don't want.
        if ($set_status) {
            $dbh->do('UPDATE bugs SET bug_status = ? WHERE bug_id = ?',
                     undef, $set_status->name, $created->id);
        }
        if ($set_resolution) {
            $dbh->do('UPDATE bugs SET resolution = ? WHERE bug_id = ?',
                     undef, $set_resolution->name, $created->id);
        }
        
        $self->_insert_comments($created, $comments);
        $self->_insert_history($created, $history);
        $self->_insert_attachments($created, $attachments);

        # bugs_fulltext isn't transactional, so if we're in a dry-run we
        # need to delete anything that we put in there.
        if ($self->dry_run) {
            $dbh->do('DELETE FROM bugs_fulltext WHERE bug_id = ?',
                     undef, $created->id);
        }

        if (!$self->verbose) {
            indicate_progress({ current => $count++, every => 5, total => $total });
        }
    }
}

sub _insert_comments {
    my ($self, $bug, $comments) = @_;
    return if !$comments;
    $self->debug(' Inserting comments:', 2);
    foreach my $comment (@$comments) {
        $self->debug($comment, 3);
        my %copy = %$comment;
        # XXX In the future, if we have a Bugzilla::Comment->create, this
        # should use it.
        my $who = Bugzilla::User->check(delete $copy{who});
        $copy{who} = $who->id;
        $copy{bug_id} = $bug->id;
        $self->_do_table_insert('longdescs', \%copy);
        $self->debug("  Inserted comment from " . $who->login, 2);
    }
    $bug->_sync_fulltext( update_comments => 1 );
}

sub _insert_history {
    my ($self, $bug, $history) = @_;
    return if !$history;
    $self->debug(' Inserting history:', 2);
    foreach my $item (@$history) {
        $self->debug($item, 3);
        my $who = Bugzilla::User->check($item->{who});
        LogActivityEntry($bug->id, $item->{field}, $item->{removed},
                         $item->{added}, $who->id, $item->{bug_when});
        $self->debug("  $item->{field} change from " . $who->login, 2);
    }
}

sub _insert_attachments {
    my ($self, $bug, $attachments) = @_;
    return if !$attachments;
    $self->debug(' Inserting attachments:', 2);
    foreach my $attachment (@$attachments) {
        $self->debug($attachment, 3);
        # Make sure that our pointer is at the beginning of the file,
        # because usually it will be at the end, having just been fully
        # written to.
        if (ref $attachment->{data}) {
            $attachment->{data}->seek(0, SEEK_SET);
        }

        my $submitter = Bugzilla::User->check(delete $attachment->{submitter});
        my $super_user = Bugzilla->user;
        # Make sure the submitter can attach this attachment no matter what.
        $submitter->{groups} = $super_user->groups;
        Bugzilla->set_user($submitter);
        my $created =
            Bugzilla::Attachment->create({ %$attachment, bug => $bug });
        $self->debug('  Attachment ' . $created->description . ' from '
                     . $submitter->login, 2);
        Bugzilla->set_user($super_user);
    }
}

sub _do_table_insert {
    my ($self, $table, $hash) = @_;
    my @fields    = keys %$hash;
    my @questions = ('?') x @fields;
    my @values    = map { $hash->{$_} } @fields;
    my $field_sql    = join(',', @fields);
    my $question_sql = join(',', @questions);
    Bugzilla->dbh->do("INSERT INTO $table ($field_sql) VALUES ($question_sql)",
                      undef, @values);
}

######################
# Helper Subroutines #
######################

sub _canonical_name {
    my ($module) = @_;
    $module =~ s{::}{/}g;
    $module = basename($module);
    $module =~ s/\.pm$//g;
    return $module;
}

1;

__END__

=head1 NAME

Bugzilla::Migrate - Functions to migrate from other databases

=head1 DESCRIPTION

This module acts as a base class for the various modules that migrate
from other bug-trackers.

The documentation for this module exists mostly to assist people in
creating new migrators for other bug-trackers than the ones currently
supported.

=head1 HOW MIGRATION WORKS

Before writing anything to the Bugzilla database, the migrator will read
everything from the other bug-tracker's database. Here's the exact order
of what happens:

=over

=item 1

Users are read from the other bug-tracker.

=item 2

Products are read from the other bug-tracker.

=item 3

Bugs are read from the other bug-tracker.

=item 4

The L</after_read> method is called.

=item 5

All bugs are translated from the other bug-tracker's fields/values
into Bugzilla's fields values using L</translate_bug>.

=item 6

Users are inserted into Bugzilla.

=item 7

Products are inserted into Bugzilla.

=item 8

Some migrators need to create custom fields before migrating, and
so that happens here.

=item 9

Any legal values that need to be created for any drop-down or
multi-select fields are created. This is done by reading all the
values on every bug that was read in and creating any values that
don't already exist in Bugzilla for every drop-down or multi-select
field on each bug. This includes creating any product versions and
milestones that need to be created.

=item 10

Bugs are inserted into Bugzilla.

=item 11

The L</after_insert> method is called.

=back

Everything happens in one big transaction, so in general, if there are
any errors during the process, nothing will be changed.

The migrator never creates anything that already exists. So users, products,
components, etc. that already exist will just be re-used by this script,
not re-created.

=head1 CONSTRUCTOR

=head2 load

Called like C<< Bugzilla::Migrate->load('Module') >>. Returns a new
C<Bugzilla::Migrate> object that can be used to migrate from the
requested bug-tracker.

=head1 METHODS YOUR SUBCLASS CAN USE

=head2 config

Takes a single parameter, a string, and returns the value of the
configuration variable with that name (always a scalar). The first time
you call C<config>, if the configuration file hasn't been read, it will
be read in.

=head2 debug

If the user hasn't specified C<--verbose> on the command line, this
does nothing.

Takes two arguments:

The first argument is a string or reference to print to C<STDERR>.
If it's a reference, L<Data::Dumper> will be used to print the
data structure.

The second argument is a number--the string will only be printed if the
user specified C<--verbose> at least that many times on the command line.

=head2 parse_date

(Note: Usually you don't need to call this, because L</translate_bug>
handles date translations for you, for bug data.)

Parses a date string and returns a formatted date string that can be inserted
into the database. If the input date is missing a timezone, the "timezone"
configuration parameter will be used as the timezone of the date.

=head2 translate_bug

(Note: Normally you don't have to call this yourself, as 
C<Bugzilla::Migrate> does it for you.)

Uses the C<$translate_fields> and C<$translate_values> configuration variables
to convert a hashref of "other bug-tracker" fields into Bugzilla fields.
It takes one argument, the hashref to convert. Any unrecognized fields will
have their value prepended to the C<comment> element in the returned
hashref, unless they are listed in L</NON_COMMENT_FIELDS>.

In scalar context, returns the translated bug. In array context,
returns both the translated bug and a second hashref containing the values
of any untranslated fields that were listed in C<NON_COMMENT_FIELDS>.

B<Note:> To save memory, the hashref that you pass in will be destroyed
(all keys will be deleted).

=head2 translate_value

(Note: Generally you only need to use this during L</_read_products>
and L</_read_users> if necessary, because the data returned from
L</_read_bugs> will be put through L</translate_bug>.)

Uses the C<$translate_values> configuration variable to convert
field values from your bug-tracker to Bugzilla. Takes two arguments,
the first being a field name and the second being a value. If the value
is an arrayref, C<translate_value> will be called recursively on all
the array elements.

Also, any date field will be converted into ISO 8601 format, for
inserting into the database.

=head2 translate_field

(Note: Normally you don't need to use this, because L</translate_bug> 
handles it for you.)

Translates a field name in your bug-tracker to a field name in Bugzilla,
using the rules described in the description of the C<$translate_fields>
configuration variable.

Takes a single argument--the name of a field to translate.

Returns C<undef> if the field could not be translated.

=head1 METHODS YOU MUST IMPLEMENT

These are methods that subclasses must implement:

=head2 _read_bugs

Should return an arrayref of hashes. The hashes will be passed to
L<Bugzilla::Bug/create> to create bugs in Bugzilla. In addition to
the normal C<create> fields, the hashes can contain three additional
items:

=over

=item comments

An arrayref of hashes, representing comments to be added to the
database. The keys should be the names of columns in the longdescs
table that you want to set for each comment. C<who> must be a
username instead of a user id, though.

You don't need to specify a value for the C<bug_id> column.

=item history

An arrayref of hashes, representing the history of changes made
to this bug. The keys should be the names of columns in the
bugs_activity table to set for each change. C<who> must be a username
instead of a user id, though, and C<field> (containing the name of some field)
is taken instead of C<fieldid>.

You don't need to specify a value for the C<bug_id> column.

=item attachments

An arrayref of hashes, representing values to pass to
L<Bugzilla::Attachment/create>. (Remember that the C<data> argument
must be a file handle--we recommend using L<IO::File/new_tmpfile> to create
anonymous temporary files for this purpose.) You should specify a
C<submitter> argument containing the username of the attachment's submitter.

You don't need to specify a value for the the C<bug> argument.

=back

=head2 _read_products

Should return an arrayref of hashes to pass to L<Bugzilla::Product/create>.
In addition to the normal C<create> fields, this also accepts an additional
argument, C<components>, which is an arrayref of hashes to pass to
L<Bugzilla::Component/create> (though you do not need to specify the
C<product> argument for L<Bugzilla::Component/create>).

=head2 _read_users

Should return an arrayref of hashes to be passed to
L<Bugzilla::User/create>.

=head1 METHODS YOU MIGHT WANT TO IMPLEMENT

These are methods that you may want to override in your migrator.
All of these methods are called on an instantiated L<Bugzilla::Migrate>
object of your subclass by L<Bugzilla::Migrate> itself.

=head2 REQUIRED_MODULES

Returns an arrayref of Perl modules that must be installed in order
for your migrator to run, in the same format as 
L<Bugzilla::Install::Requirements/REQUIRED_MODULES>.

=head2 CUSTOM_FIELDS

Returns a hashref, where the keys are the names of custom fields
to create in the database before inserting bugs. The values of the
hashref are the arguments (other than "name") that should be passed
to Bugzilla::Field->create() when creating the field. (C<< custom => 1 >>
will be specified automatically for you, so you don't need to specify it.)

=head2 CONFIG_VARS

This should return an array (not an arrayref) in the same format as
L<Bugzilla::Install::Localconfig/LOCALCONFIG_VARS>, describing
configuration variables for migrating from your bug-tracker. You should
always include the default C<CONFIG_VARS> (by calling
$self->SUPER::CONFIG_VARS) as part of your return value, if you
override this method.

=head2 NON_COMMENT_FIELDS

An array (not an arrayref). If there are fields that are not translated
and yet shouldn't be added to the initial description of the bug when
translating bugs, then they should be listed here. See L</translate_bug> for
more detail.

=head2 before_read

This is called before any data is read from the "other bug-tracker".
The default implementation does nothing.

=head2 after_read

This is run after all data is read from the other bug-tracker, but
before the bug fields/values have been translated, and before any data
is inserted into Bugzilla. The default implementation does nothing.

=head2 before_insert

This is called after all bugs are translated from their "other bug-tracker"
values to Bugzilla values, but before any data is inserted into the database
or any custom fields are created. The default implementation does nothing.

=head2 after_insert

This is run after all data is inserted into Bugzilla. The default
implementation does nothing.

=head1 B<Methods in need of POD>

=over

=item do_migration

=item verbose

=item bug_fields

=item insert_users

=item users

=item check_requirements

=item bugs

=item map_value

=item insert_products

=item products

=item translate_all_bugs

=item config_file_name

=item dry_run

=item name

=item create_custom_fields

=item reset_serial_values

=item read_config

=item write_config

=item insert_bugs

=item create_legal_values

=back
