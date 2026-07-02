# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::Example;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Extension);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Group;
use Bugzilla::User;
use Bugzilla::User::Setting;
use Bugzilla::Util qw(diff_arrays html_quote remote_ip);
use Bugzilla::Status qw(is_open_state);
use Bugzilla::Install::Filesystem;
use Bugzilla::WebService::Constants;

# This is extensions/Example/lib/Util.pm. I can load this here in my
# Extension.pm only because I have a Config.pm.
use Bugzilla::Extension::Example::Util;

use Data::Dumper;

# See bugmail_relationships.
use constant REL_EXAMPLE => -127;

our $VERSION = '1.0';

sub user_can_administer {
    my ($self, $args) = @_;
    my $can_administer = $args->{can_administer};

    # If you add an option to the admin pages (e.g. by using the Hooks in
    # template/en/default/admin/admin.html.tmpl), you may want to allow
    # users in another group view admin.cgi
    #if (Bugzilla->user->in_group('other_group')) {
    #    $$can_administer = 1;
    #}
}

sub admin_editusers_action {
    my ($self, $args) = @_;
    my ($vars, $action, $user) = @$args{qw(vars action user)};
    my $template = Bugzilla->template;

    if ($action eq 'my_action') {
        # Allow to restrict the search to any group the user is allowed to bless.
        $vars->{'restrictablegroups'} = $user->bless_groups();
        $template->process('admin/users/search.html.tmpl', $vars)
            || ThrowTemplateError($template->error());
        exit;
    }
}

sub attachment_process_data {
    my ($self, $args) = @_;
    my $type     = $args->{attributes}->{mimetype};
    my $filename = $args->{attributes}->{filename};

    # Make sure images have the correct extension.
    # Uncomment the two lines below to make this check effective.
    if ($type =~ /^image\/(\w+)$/) {
        my $format = $1;
        if ($filename =~ /^(.+)(:?\.[^\.]+)$/) {
            my $name = $1;
            #$args->{attributes}->{filename} = "${name}.$format";
        }
        else {
            # The file has no extension. We append it.
            #$args->{attributes}->{filename} .= ".$format";
        }
    }
}

sub auth_login_methods {
    my ($self, $args) = @_;
    my $modules = $args->{modules};
    if (exists $modules->{Example}) {
        $modules->{Example} = 'Bugzilla/Extension/Example/Auth/Login.pm';
    }
}

sub auth_verify_methods {
    my ($self, $args) = @_;
    my $modules = $args->{modules};
    if (exists $modules->{Example}) {
        $modules->{Example} = 'Bugzilla/Extension/Example/Auth/Verify.pm';
    }
}

sub bug_check_can_change_field {
    my ($self, $args) = @_;

    my ($bug, $field, $new_value, $old_value, $priv_results)
        = @$args{qw(bug field new_value old_value priv_results)};

    my $user = Bugzilla->user;

    # Disallow a bug from being reopened if currently closed unless user 
    # is in 'admin' group
    if ($field eq 'bug_status' && $bug->product_obj->name eq 'Example') {
        if (!is_open_state($old_value) && is_open_state($new_value) 
            && !$user->in_group('admin')) 
        {
            push(@$priv_results, PRIVILEGES_REQUIRED_EMPOWERED);
            return;
        }
    }

    # Disallow a bug's keywords from being edited unless user is the
    # reporter of the bug 
    if ($field eq 'keywords' && $bug->product_obj->name eq 'Example' 
        && $user->login ne $bug->reporter->login) 
    {
        push(@$priv_results, PRIVILEGES_REQUIRED_REPORTER);
        return;
    }

    # Allow updating of priority even if user cannot normally edit the bug 
    # and they are in group 'engineering'
    if ($field eq 'priority' && $bug->product_obj->name eq 'Example'
        && $user->in_group('engineering')) 
    {
        push(@$priv_results, PRIVILEGES_REQUIRED_NONE);
        return;
    }
}

sub bug_columns {
    my ($self, $args) = @_;
    my $columns = $args->{'columns'};
    push (@$columns, "delta_ts AS example")
}

sub bug_end_of_create {
    my ($self, $args) = @_;

    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.
    my $bug = $args->{'bug'};
    my $timestamp = $args->{'timestamp'};
    
    my $bug_id = $bug->id;
    # Uncomment this line to see a line in your webserver's error log whenever
    # you file a bug.
    # warn "Bug $bug_id has been filed!";
}

sub bug_end_of_create_validators {
    my ($self, $args) = @_;
    
    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.
    my $bug_params = $args->{'params'};
    
    # Uncomment this line below to see a line in your webserver's error log
    # containing all validated bug field values every time you file a bug.
    # warn Dumper($bug_params);
    
    # This would remove all ccs from the bug, preventing ANY ccs from being
    # added on bug creation.
    # $bug_params->{cc} = [];
}

sub bug_start_of_update {
    my ($self, $args) = @_;

    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.
    my ($bug, $old_bug, $timestamp, $changes) = 
        @$args{qw(bug old_bug timestamp changes)};

    foreach my $field (keys %$changes) {
        my $used_to_be = $changes->{$field}->[0];
        my $now_it_is  = $changes->{$field}->[1];
    }

    my $old_summary = $old_bug->short_desc;

    my $status_message;
    if (my $status_change = $changes->{'bug_status'}) {
        my $old_status = new Bugzilla::Status({ name => $status_change->[0] });
        my $new_status = new Bugzilla::Status({ name => $status_change->[1] });
        if ($new_status->is_open && !$old_status->is_open) {
            $status_message = "Bug re-opened!";
        }
        if (!$new_status->is_open && $old_status->is_open) {
            $status_message = "Bug closed!";
        }
    }

    my $bug_id = $bug->id;
    my $num_changes = scalar keys %$changes;
    my $result = "There were $num_changes changes to fields on bug $bug_id"
                 . " at $timestamp.";
    # Uncomment this line to see $result in your webserver's error log whenever
    # you update a bug.
    # warn $result;
}

sub bug_end_of_update {
    my ($self, $args) = @_;
    
    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.
    my ($bug, $old_bug, $timestamp, $changes) = 
        @$args{qw(bug old_bug timestamp changes)};
    
    foreach my $field (keys %$changes) {
        my $used_to_be = $changes->{$field}->[0];
        my $now_it_is  = $changes->{$field}->[1];
    }

    my $old_summary = $old_bug->short_desc;

    my $status_message;
    if (my $status_change = $changes->{'bug_status'}) {
        my $old_status = new Bugzilla::Status({ name => $status_change->[0] });
        my $new_status = new Bugzilla::Status({ name => $status_change->[1] });
        if ($new_status->is_open && !$old_status->is_open) {
            $status_message = "Bug re-opened!";
        }
        if (!$new_status->is_open && $old_status->is_open) {
            $status_message = "Bug closed!";
        }
    }
    
    my $bug_id = $bug->id;
    my $num_changes = scalar keys %$changes;
    my $result = "There were $num_changes changes to fields on bug $bug_id"
                 . " at $timestamp.";
    # Uncomment this line to see $result in your webserver's error log whenever
    # you update a bug.
    # warn $result;
}

sub bug_fields {
    my ($self, $args) = @_;

    my $fields = $args->{'fields'};
    push (@$fields, "example")
}

sub bug_format_comment {
    my ($self, $args) = @_;
    
    # This replaces every occurrence of the word "foo" with the word
    # "bar"
    
    my $regexes = $args->{'regexes'};
    push(@$regexes, { match => qr/\bfoo\b/, replace => 'bar' });
    
    # And this links every occurrence of the word "bar" to example.com,
    # but it won't affect "foo"s that have already been turned into "bar"
    # above (because each regex is run in order, and later regexes don't modify
    # earlier matches, due to some cleverness in Bugzilla's internals).
    #
    # For example, the phrase "foo bar" would become:
    # bar <a href="http://example.com/bar">bar</a>
    my $bar_match = qr/\b(bar)\b/;
    push(@$regexes, { match => $bar_match, replace => \&_replace_bar });
}

# Used by bug_format_comment--see its code for an explanation.
sub _replace_bar {
    my $args = shift;
    # $match is the first parentheses match in the $bar_match regex 
    # in bug-format_comment.pl. We get up to 10 regex matches as 
    # arguments to this function.
    my $match = $args->{matches}->[0];
    # Remember, you have to HTML-escape any data that you are returning!
    $match = html_quote($match);
    return qq{<a href="http://example.com/">$match</a>};
};

sub buglist_columns {
    my ($self, $args) = @_;
    
    my $columns = $args->{'columns'};
    $columns->{'example'} = { 'name' => 'bugs.delta_ts' , 'title' => 'Example' };
    $columns->{'product_desc'} = { 'name'  => 'prod_desc.description',
                                   'title' => 'Product Description' };
}

sub buglist_column_joins {
    my ($self, $args) = @_;
    my $joins = $args->{'column_joins'};

    # This column is added using the "buglist_columns" hook
    $joins->{'product_desc'} = {
        from  => 'product_id',
        to    => 'id',
        table => 'products',
        as    => 'prod_desc',
        join  => 'INNER',
    };
}

sub search_operator_field_override {
    my ($self, $args) = @_;
    
    my $operators = $args->{'operators'};

    my $original = $operators->{component}->{_non_changed};
    $operators->{component} = {
        _non_changed => sub { _component_nonchanged($original, @_) }
    };
}

sub _component_nonchanged {
    my $original = shift;
    my ($invocant, $args) = @_;

    $invocant->$original($args);
    # Actually, it does not change anything in the result,
    # just an example.
    $args->{term} = $args->{term} . " OR 1=2";
}

sub bugmail_recipients {
    my ($self, $args) = @_;
    my $recipients = $args->{recipients};
    my $bug = $args->{bug};

    my $user = 
        new Bugzilla::User({ name => Bugzilla->params->{'maintainer'} });

    if ($bug->id == 1) {
        # Uncomment the line below to add the maintainer to the recipients
        # list of every bugmail from bug 1 as though that the maintainer
        # were on the CC list.
        #$recipients->{$user->id}->{+REL_CC} = 1;

        # And this line adds the maintainer as though they had the
        # "REL_EXAMPLE" relationship from the bugmail_relationships hook below.
        #$recipients->{$user->id}->{+REL_EXAMPLE} = 1;
    }
}

sub bugmail_relationships {
    my ($self, $args) = @_;
    my $relationships = $args->{relationships};
    $relationships->{+REL_EXAMPLE} = 'Example';
}

sub cgi_headers {
    my ($self, $args) = @_;
    my $headers = $args->{'headers'};

    $headers->{'-x_test_header'} = "Test header from Example extension";
}

sub config_add_panels {
    my ($self, $args) = @_;
    
    my $modules = $args->{panel_modules};
    $modules->{Example} = "Bugzilla::Extension::Example::Config";
}

sub config_modify_panels {
    my ($self, $args) = @_;
    
    my $panels = $args->{panels};
    
    # Add the "Example" auth methods.
    my $auth_params = $panels->{'auth'}->{params};
    my ($info_class)   = grep($_->{name} eq 'user_info_class', @$auth_params);
    my ($verify_class) = grep($_->{name} eq 'user_verify_class', @$auth_params);

    push(@{ $info_class->{choices} },   'CGI,Example');
    push(@{ $verify_class->{choices} }, 'Example');

    push(@$auth_params, { name => 'param_example',
                          type => 't',
                          default => 0,
                          checker => \&check_numeric });    
}

sub db_schema_abstract_schema {
    my ($self, $args) = @_;
#    $args->{'schema'}->{'example_table'} = {
#        FIELDS => [
#            id       => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
#                     PRIMARYKEY => 1},
#            for_key  => {TYPE => 'INT3', NOTNULL => 1,
#                           REFERENCES  => {TABLE  =>  'example_table2',
#                                           COLUMN =>  'id',
#                                           DELETE => 'CASCADE'}},
#            col_3    => {TYPE => 'varchar(64)', NOTNULL => 1},
#        ],
#        INDEXES => [
#            id_index_idx   => {FIELDS => ['col_3'], TYPE => 'UNIQUE'},
#            for_id_idx => ['for_key'],
#        ],
#    };
}

sub email_in_before_parse {
    my ($self, $args) = @_;

    my $subject = $args->{mail}->header('Subject');
    # Correctly extract the bug ID from email subjects of the form [Bug comp/NNN].
    if ($subject =~ /\[.*(\d+)\].*/) {
        $args->{fields}->{bug_id} = $1;
    }
}

sub email_in_after_parse {
    my ($self, $args) = @_;
    my $reporter = $args->{fields}->{reporter};
    my $dbh = Bugzilla->dbh;

    # No other check needed if this is a valid regular user.
    return if login_to_id($reporter);

    # The reporter is not a regular user. We create an account for them,
    # but they can only comment on existing bugs.
    # This is useful for people who reply by email to bugmails received
    # in mailing-lists.
    if ($args->{fields}->{bug_id}) {
        # WARNING: we return now to skip the remaining code below.
        # You must understand that removing this line would make the code
        # below effective! Do it only if you are OK with the behavior
        # described here.
        return;

        Bugzilla::User->create({ login_name => $reporter, cryptpassword => '*' });

        # For security reasons, delete all fields unrelated to comments.
        foreach my $field (keys %{$args->{fields}}) {
            next if $field =~ /^(?:bug_id|comment|reporter)$/;
            delete $args->{fields}->{$field};
        }
    }
    else {
        ThrowUserError('invalid_username', { name => $reporter });
    }
}

sub enter_bug_entrydefaultvars {
    my ($self, $args) = @_;
    
    my $vars = $args->{vars};
    $vars->{'example'} = 1;
}

sub error_catch {
    my ($self, $args) = @_;
    # Customize the error message displayed when someone tries to access
    # page.cgi with an invalid page ID, and keep track of this attempt
    # in the web server log.
    return unless Bugzilla->error_mode == ERROR_MODE_WEBPAGE;
    return unless $args->{error} eq 'bad_page_cgi_id';

    my $page_id = $args->{vars}->{page_id};
    my $login = Bugzilla->user->identity || "Someone";
    warn "$login attempted to access page.cgi with id = $page_id";

    my $page = $args->{message};
    my $new_error_msg = "Ah ah, you tried to access $page_id? Good try!";
    $new_error_msg = html_quote($new_error_msg);
    # There are better tools to parse an HTML page, but it's just an example.
    # Since Perl 5.16, we can no longer write "class" inside look-behind
    # assertions, because "ss" is also seen as the german ß character, which
    # makes Perl 5.16 complain. The right fix is to use the /aa modifier,
    # but it's only understood since Perl 5.14. So the workaround is to write
    # "clas[s]" instead of "class". Stupid and ugly hack, but it works with
    # all Perl versions.
    $$page =~ s/(?<=<td id="error_msg" clas[s]="throw_error">).*(?=<\/td>)/$new_error_msg/si;
}

sub flag_end_of_update {
    my ($self, $args) = @_;
    
    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.
    my $flag_params = $args;
    my ($object, $timestamp, $old_flags, $new_flags) =
        @$flag_params{qw(object timestamp old_flags new_flags)};
    my ($removed, $added) = diff_arrays($old_flags, $new_flags);
    my ($granted, $denied) = (0, 0);
    foreach my $new_flag (@$added) {
        $granted++ if $new_flag =~ /\+$/;
        $denied++ if $new_flag =~ /-$/;
    }
    my $bug_id = $object->isa('Bugzilla::Bug') ? $object->id 
                                               : $object->bug_id;
    my $result = "$granted flags were granted and $denied flags were denied"
                 . " on bug $bug_id at $timestamp.";
    # Uncomment this line to see $result in your webserver's error log whenever
    # you update flags.
    # warn $result;
}

sub group_before_delete {
    my ($self, $args) = @_;
    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.

    my $group = $args->{'group'};
    my $group_id = $group->id;
    # Uncomment this line to see a line in your webserver's error log whenever
    # you file a bug.
    # warn "Group $group_id is about to be deleted!";
}

sub group_end_of_create {
    my ($self, $args) = @_;
    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.
    my $group = $args->{'group'};

    my $group_id = $group->id;
    # Uncomment this line to see a line in your webserver's error log whenever
    # you create a new group.
    #warn "Group $group_id has been created!";
}

sub group_end_of_update {
    my ($self, $args) = @_;
    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.

    my ($group, $changes) = @$args{qw(group changes)};

    foreach my $field (keys %$changes) {
        my $used_to_be = $changes->{$field}->[0];
        my $now_it_is  = $changes->{$field}->[1];
    }

    my $group_id = $group->id;
    my $num_changes = scalar keys %$changes;
    my $result = 
        "There were $num_changes changes to fields on group $group_id.";
    # Uncomment this line to see $result in your webserver's error log whenever
    # you update a group.
    #warn $result;
}

sub install_before_final_checks {
    my ($self, $args) = @_;
    print "Install-before_final_checks hook\n" unless $args->{silent};
    
    # Add a new user setting like this:
    #
    # add_setting('product_chooser',           # setting name
    #             ['pretty', 'full', 'small'], # options
    #             'pretty');                   # default
    #
    # To add descriptions for the setting and choices, add extra values to 
    # the hash defined in global/setting-descs.none.tmpl. Do this in a hook: 
    # hook/global/setting-descs-settings.none.tmpl .
}

sub install_filesystem {
    my ($self, $args) = @_;
    my $create_dirs  = $args->{'create_dirs'};
    my $recurse_dirs = $args->{'recurse_dirs'};
    my $htaccess     = $args->{'htaccess'};

    # Create a new directory in datadir specifically for this extension.
    # The directory will need to allow files to be created by the extension
    # code as well as allow the webserver to server content from it.
    # my $data_path = bz_locations->{'datadir'} . "/" . __PACKAGE__->NAME;
    # $create_dirs->{$data_path} = Bugzilla::Install::Filesystem::DIR_CGI_WRITE;
   
    # Update the permissions of any files and directories that currently reside
    # in the extension's directory. 
    # $recurse_dirs->{$data_path} = {
    #     files => Bugzilla::Install::Filesystem::CGI_READ,
    #     dirs  => Bugzilla::Install::Filesystem::DIR_CGI_WRITE
    # };
    
    # Create a htaccess file that allows specific content to be served from the 
    # extension's directory.
    # $htaccess->{"$data_path/.htaccess"} = {
    #     perms    => Bugzilla::Install::Filesystem::WS_SERVE,
    #     contents => Bugzilla::Install::Filesystem::HT_DEFAULT_DENY
    # };
}

sub install_update_db {
    my $dbh = Bugzilla->dbh;
#    $dbh->bz_add_column('example', 'new_column',
#                        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0});
#    $dbh->bz_add_index('example', 'example_new_column_idx', [qw(value)]);
}

sub install_update_db_fielddefs {
    my $dbh = Bugzilla->dbh;
#    $dbh->bz_add_column('fielddefs', 'example_column', 
#                        {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => ''});
}

sub job_map {
    my ($self, $args) = @_;
    
    my $job_map = $args->{job_map};
    
    # This adds the named class (an instance of TheSchwartz::Worker) as a
    # handler for when a job is added with the name "some_task".
    $job_map->{'some_task'} = 'Bugzilla::Extension::Example::Job::SomeClass';
    
    # Schedule a job like this:
    # my $queue = Bugzilla->job_queue();
    # $queue->insert('some_task', { some_parameter => $some_variable });
}

sub mailer_before_send {
    my ($self, $args) = @_;
    
    my $email = $args->{email};
    # If you add a header to an email, it's best to start it with
    # 'X-Bugzilla-<Extension>' so that you don't conflict with
    # other extensions.
    $email->header_set('X-Bugzilla-Example-Header', 'Example');
}

sub object_before_create {
    my ($self, $args) = @_;
    
    my $class = $args->{'class'};
    my $object_params = $args->{'params'};
    
    # Note that this is a made-up class, for this example.
    if ($class->isa('Bugzilla::ExampleObject')) {
        warn "About to create an ExampleObject!";
        warn "Got the following parameters: " 
             . join(', ', keys(%$object_params));
    }
}

sub object_before_delete {
    my ($self, $args) = @_;

    my $object = $args->{'object'};

    # Note that this is a made-up class, for this example.
    if ($object->isa('Bugzilla::ExampleObject')) {
        my $id = $object->id;
        warn "An object with id $id is about to be deleted!";
    } 
}

sub object_before_set {
    my ($self, $args) = @_;
    
    my ($object, $field, $value) = @$args{qw(object field value)};
    
    # Note that this is a made-up class, for this example.
    if ($object->isa('Bugzilla::ExampleObject')) {
        warn "The field $field is changing from " . $object->{$field} 
             . " to $value!";
    }
}

sub object_columns {
    my ($self, $args) = @_;
    my ($class, $columns) = @$args{qw(class columns)};

    if ($class->isa('Bugzilla::ExampleObject')) {
        push(@$columns, 'example');
    }
}

sub object_end_of_create {
    my ($self, $args) = @_;
    
    my $class  = $args->{'class'};
    my $object = $args->{'object'};

    warn "Created a new $class object!";
}

sub object_end_of_create_validators {
    my ($self, $args) = @_;
    
    my $class = $args->{'class'};
    my $object_params = $args->{'params'};
    
    # Note that this is a made-up class, for this example.
    if ($class->isa('Bugzilla::ExampleObject')) {
        # Always set example_field to 1, even if the validators said otherwise.
        $object_params->{example_field} = 1;
    }
    
}

sub object_end_of_set {
    my ($self, $args) = @_;

    my ($object, $field) = @$args{qw(object field)};

    # Note that this is a made-up class, for this example.
    if ($object->isa('Bugzilla::ExampleObject')) {
        warn "The field $field has changed to " . $object->{$field};
    }
}

sub object_end_of_set_all {
    my ($self, $args) = @_;
    
    my $object = $args->{'object'};
    my $object_params = $args->{'params'};
    
    # Note that this is a made-up class, for this example.
    if ($object->isa('Bugzilla::ExampleObject')) {
        if ($object_params->{example_field} == 1) {
            $object->{example_field} = 1;
        }
    }
    
}

sub object_end_of_update {
    my ($self, $args) = @_;
    
    my ($object, $old_object, $changes) = 
        @$args{qw(object old_object changes)};
    
    # Note that this is a made-up class, for this example.
    if ($object->isa('Bugzilla::ExampleObject')) {
        if (defined $changes->{'name'}) {
            my ($old, $new) = @{ $changes->{'name'} };
            print "The name field changed from $old to $new!";
        }
    }
}

sub object_update_columns {
    my ($self, $args) = @_;
    my ($object, $columns) = @$args{qw(object columns)};

    if ($object->isa('Bugzilla::ExampleObject')) {
        push(@$columns, 'example');
    }
}

sub object_validators {
    my ($self, $args) = @_;
    my ($class, $validators) = @$args{qw(class validators)};

    if ($class->isa('Bugzilla::Bug')) {
        # This is an example of adding a new validator.
        # See the _check_example subroutine below.
        $validators->{example} = \&_check_example;

        # This is an example of overriding an existing validator.
        # See the check_short_desc validator below.
        my $original = $validators->{short_desc};
        $validators->{short_desc} = sub { _check_short_desc($original, @_) };
    }
}

sub _check_example {
    my ($invocant, $value, $field) = @_;
    warn "I was called to validate the value of $field.";
    warn "The value of $field that I was passed in is: $value";

    # Make the value always be 1.
    my $fixed_value = 1;
    return $fixed_value;
}

sub _check_short_desc {
    my $original = shift;
    my $invocant = shift;
    my $value = $invocant->$original(@_);
    if ($value !~ /example/i) {
        # Use this line to make Bugzilla throw an error every time
        # you try to file a bug or update a bug without the word "example"
        # in the summary.
        if (0) {
            ThrowUserError('example_short_desc_invalid');
        }
    }
    return $value;
}

sub page_before_template {
    my ($self, $args) = @_;
    
    my ($vars, $page) = @$args{qw(vars page_id)};
    
    # You can see this hook in action by loading page.cgi?id=example.html
    if ($page eq 'example.html') {
        $vars->{cgi_variables} = { Bugzilla->cgi->Vars };
    }
}

sub path_info_whitelist {
    my ($self, $args) = @_;
    my $whitelist = $args->{whitelist};
    push(@$whitelist, "page.cgi");
}

sub post_bug_after_creation {
    my ($self, $args) = @_;
    
    my $vars = $args->{vars};
    $vars->{'example'} = 1;
}

sub product_confirm_delete {
    my ($self, $args) = @_;
    
    my $vars = $args->{vars};
    $vars->{'example'} = 1;
}


sub product_end_of_create {
    my ($self, $args) = @_;

    my $product = $args->{product};

    # For this example, any lines of code that actually make changes to your
    # database have been commented out.

    # This section will take a group that exists in your installation
    # (possible called test_group) and automatically makes the new
    # product hidden to only members of the group. Just remove
    # the restriction if you want the new product to be public.

    my $example_group = new Bugzilla::Group({ name => 'example_group' });

    if ($example_group) {
        $product->set_group_controls($example_group, 
                { entry          => 1,
                  membercontrol  => CONTROLMAPMANDATORY,
                  othercontrol   => CONTROLMAPMANDATORY });
#        $product->update();
    }

    # This section will automatically add a default component
    # to the new product called 'No Component'.

    my $default_assignee = new Bugzilla::User(
        { name => Bugzilla->params->{maintainer} });

    if ($default_assignee) {
#        Bugzilla::Component->create(
#            { name             => 'No Component',
#              product          => $product,
#              description      => 'Select this component if one does not ' . 
#                                  'exist in the current list of components',
#              initialowner     => $default_assignee });
    }
}

sub quicksearch_map {
    my ($self, $args) = @_;
    my $map = $args->{'map'};

    # This demonstrates adding a shorter alias for a long custom field name.
    $map->{'impact'} = $map->{'cf_long_field_name_for_impact_field'};
}

sub sanitycheck_check {
    my ($self, $args) = @_;
    
    my $dbh = Bugzilla->dbh;
    my $sth;
    
    my $status = $args->{'status'};
    
    # Check that all users are Australian
    $status->('example_check_au_user');
    
    $sth = $dbh->prepare("SELECT userid, login_name
                            FROM profiles
                           WHERE login_name NOT LIKE '%.au'");
    $sth->execute;
    
    my $seen_nonau = 0;
    while (my ($userid, $login, $numgroups) = $sth->fetchrow_array) {
        $status->('example_check_au_user_alert',
                  { userid => $userid, login => $login },
                  'alert');
        $seen_nonau = 1;
    }
    
    $status->('example_check_au_user_prompt') if $seen_nonau;
}

sub sanitycheck_repair {
    my ($self, $args) = @_;
    
    my $cgi = Bugzilla->cgi;
    my $dbh = Bugzilla->dbh;
    
    my $status = $args->{'status'};
    
    if ($cgi->param('example_repair_au_user')) {
        $status->('example_repair_au_user_start');
    
        #$dbh->do("UPDATE profiles
        #             SET login_name = CONCAT(login_name, '.au')
        #           WHERE login_name NOT LIKE '%.au'");
    
        $status->('example_repair_au_user_end');
    }
}

sub template_before_create {
    my ($self, $args) = @_;
    
    my $config = $args->{'config'};
    # This will be accessible as "example_global_variable" in every
    # template in Bugzilla. See Bugzilla/Template.pm's create() function
    # for more things that you can set.
    $config->{VARIABLES}->{example_global_variable} = sub { return 'value' };
}

sub template_before_process {
    my ($self, $args) = @_;
    
    my ($vars, $file, $context) = @$args{qw(vars file context)};

    if ($file eq 'bug/edit.html.tmpl') {
        $vars->{'viewing_the_bug_form'} = 1;
    }
}

sub user_check_account_creation {
    my ($self, $args) = @_;

    my $login = $args->{login};
    my $ip = remote_ip();

    # Log all requests.
    warn "USER ACCOUNT CREATION REQUEST FOR $login ($ip)";

    # Reject requests based on their email address.
    if ($login =~ /\@evil\.com$/) {
        ThrowUserError('account_creation_restricted');
    }

    # Reject requests based on their IP address.
    if ($ip =~ /^192\.168\./) {
        ThrowUserError('account_creation_restricted');
    }
}

sub user_preferences {
    my ($self, $args) = @_;
    my $tab = $args->{current_tab};
    my $save = $args->{save_changes};
    my $handled = $args->{handled};

    return unless $tab eq 'my_tab';

    my $value = Bugzilla->input_params->{'example_pref'};
    if ($save) {
        # Validate your data and update the DB accordingly.
        $value =~ s/\s+/:/g;
    }
    $args->{'vars'}->{example_pref} = $value;

    # Set the 'handled' scalar reference to true so that the caller
    # knows the panel name is valid and that an extension took care of it.
    $$handled = 1;
}

sub webservice {
    my ($self, $args) = @_;

    my $dispatch = $args->{dispatch};
    $dispatch->{Example} = "Bugzilla::Extension::Example::WebService";
}

sub webservice_error_codes {
    my ($self, $args) = @_;

    my $error_map = $args->{error_map};
    $error_map->{'example_my_error'} = 10001;
}

sub webservice_status_code_map {
    my ($self, $args) = @_;

    my $status_code_map = $args->{status_code_map};
    # Uncomment this line to override the status code for the
    # error 'object_does_not_exist' to STATUS_BAD_REQUEST
    #$status_code_map->{51} = STATUS_BAD_REQUEST;
}

sub webservice_before_call {
    my ($self, $args) = @_;

    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.
    my $method      = $args->{method};
    my $full_method = $args->{full_method};

    # Uncomment this line to see a line in your webserver's error log whenever
    # a webservice call is made
    #warn "RPC call $full_method made by ",
    #   Bugzilla->user->login || 'an anonymous user', "\n";
}

sub webservice_fix_credentials {
    my ($self, $args) = @_;
    my $rpc    = $args->{'rpc'};
    my $params = $args->{'params'};
    # Allow user to pass in username=foo&password=bar
    if (exists $params->{'username'} && exists $params->{'password'}) {
        $params->{'Bugzilla_login'}    = $params->{'username'};
        $params->{'Bugzilla_password'} = $params->{'password'};
    }
}

sub webservice_rest_request {
    my ($self, $args) = @_;
    my $rpc    = $args->{'rpc'};
    my $params = $args->{'params'};
    # Internally we may have a field called 'cf_test_field' but we allow users
    # to use the shorter 'test_field' name.
    if (exists $params->{'test_field'}) {
        $params->{'test_field'} = delete $params->{'cf_test_field'};
    }
}

sub webservice_rest_resources {
    my ($self, $args) = @_;
    my $rpc = $args->{'rpc'};
    my $resources = $args->{'resources'};
    # Add a new resource that allows for /rest/example/hello
    # to call Example.hello
    $resources->{'Bugzilla::Extension::Example::WebService'} = [
        qr{^/example/hello$}, {
            GET => {
                method => 'hello',
            }
        }
    ];
}

sub webservice_rest_response {
    my ($self, $args) = @_;
    my $rpc      = $args->{'rpc'};
    my $result   = $args->{'result'};
    my $response = $args->{'response'};
    # Convert a list of bug hashes to a single bug hash if only one is
    # being returned.
    if (ref $$result eq 'HASH'
        && exists $$result->{'bugs'}
        && scalar @{ $$result->{'bugs'} } == 1)
    {
        $$result = $$result->{'bugs'}->[0];
    }
}

# This must be the last line of your extension.
__PACKAGE__->NAME;
