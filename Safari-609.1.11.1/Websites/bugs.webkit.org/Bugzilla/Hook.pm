# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Hook;

use 5.10.1;
use strict;
use warnings;

sub process {
    my ($name, $args) = @_;

    _entering($name);

    foreach my $extension (@{ Bugzilla->extensions }) {
        if ($extension->can($name)) {
            $extension->$name($args);
        }
    }

    _leaving($name);
}

sub in {
    my $hook_name = shift;
    my $currently_in = Bugzilla->request_cache->{hook_stack}->[-1] || '';
    return $hook_name eq $currently_in ? 1 : 0;
}

sub _entering {
    my ($hook_name) = @_;
    my $hook_stack = Bugzilla->request_cache->{hook_stack} ||= [];
    push(@$hook_stack, $hook_name);
}

sub _leaving {
    pop @{ Bugzilla->request_cache->{hook_stack} };
}

1;

__END__

=head1 NAME

Bugzilla::Hook - Extendable extension hooks for Bugzilla code

=head1 SYNOPSIS

 use Bugzilla::Hook;

 Bugzilla::Hook::process("hookname", { arg => $value, arg2 => $value2 });

=head1 DESCRIPTION

Bugzilla allows extension modules to drop in and add routines at 
arbitrary points in Bugzilla code. These points are referred to as
hooks. When a piece of standard Bugzilla code wants to allow an extension
to perform additional functions, it uses Bugzilla::Hook's L</process>
subroutine to invoke any extension code if installed. 

The implementation of extensions is described in L<Bugzilla::Extension>.

There is sample code for every hook in the Example extension, located in
F<extensions/Example/Extension.pm>.

=head2 How Hooks Work

When a hook named C<HOOK_NAME> is run, Bugzilla looks through all
enabled L<extensions|Bugzilla::Extension> for extensions that implement
a subroutine named C<HOOK_NAME>.

See L<Bugzilla::Extension> for more details about how an extension
can run code during a hook.

=head1 SUBROUTINES

=over

=item C<process>

=over

=item B<Description>

Invoke any code hooks with a matching name from any installed extensions.

See L<Bugzilla::Extension> for more information on Bugzilla's extension
mechanism.

=item B<Params>

=over

=item C<$name> - The name of the hook to invoke.

=item C<$args> - A hashref. The named args to pass to the hook. 
They will be passed as arguments to the hook method in the extension.

=back

=item B<Returns> (nothing)

=back

=back

=head1 HOOKS

This describes what hooks exist in Bugzilla currently. They are mostly
in alphabetical order, but some related hooks are near each other instead
of being alphabetical.

=head2 admin_editusers_action

This hook allows you to add additional actions to the admin Users page.

Params:

=over

=item C<vars>

You can add as many new key/value pairs as you want to this hashref.
It will be passed to the template.

=item C<action>

A text which indicates the different behaviors that editusers.cgi will have.
With this hook you can change the behavior of an action or add new actions.

=item C<user>

This is a Bugzilla::User object of the user.

=back

=head2 attachment_process_data

This happens at the very beginning process of the attachment creation.
You can edit the attachment content itself as well as all attributes
of the attachment, before they are validated and inserted into the DB.

Params:

=over

=item C<data> - A reference pointing either to the content of the file
being uploaded or pointing to the filehandle associated with the file.

=item C<attributes> - A hashref whose keys are the same as the input to
L<Bugzilla::Attachment/create>. The data in this hashref hasn't been validated
yet.

=back

=head2 auth_login_methods

This allows you to add new login types to Bugzilla.
(See L<Bugzilla::Auth::Login>.)

Params:

=over

=item C<modules>

This is a hash--a mapping from login-type "names" to the actual module on
disk. The keys will be all the values that were passed to 
L<Bugzilla::Auth/login> for the C<Login> parameter. The values are the
actual path to the module on disk. (For example, if the key is C<DB>, the
value is F<Bugzilla/Auth/Login/DB.pm>.)

For your extension, the path will start with 
F<Bugzilla/Extension/Foo/>, where "Foo" is the name of your Extension. 
(See the code in the example extension.)

If your login type is in the hash as a key, you should set that key to the
right path to your module. That module's C<new> method will be called,
probably with empty parameters. If your login type is I<not> in the hash,
you should not set it.

You will be prevented from adding new keys to the hash, so make sure your
key is in there before you modify it. (In other words, you can't add in
login methods that weren't passed to L<Bugzilla::Auth/login>.)

=back

=head2 auth_verify_methods

This works just like L</auth_login_methods> except it's for
login verification methods (See L<Bugzilla::Auth::Verify>.) It also
takes a C<modules> parameter, just like L</auth_login_methods>.

=head2 bug_columns

B<DEPRECATED> Use L</object_columns> instead.

This allows you to add new fields that will show up in every L<Bugzilla::Bug>
object. Note that you will also need to use the L</bug_fields> hook in
conjunction with this hook to make this work.

Params:

=over

=item C<columns> - An arrayref containing an array of column names. Push
your column name(s) onto the array.

=back

=head2 bug_end_of_create

This happens at the end of L<Bugzilla::Bug/create>, after all other changes are
made to the database. This occurs inside a database transaction.

Params:

=over

=item C<bug> - The created bug object.

=item C<timestamp> - The timestamp used for all updates in this transaction,
as a SQL date string.

=back

=head2 bug_end_of_create_validators

This happens during L<Bugzilla::Bug/create>, after all parameters have
been validated, but before anything has been inserted into the database.

Params:

=over

=item C<params>

A hashref. The validated parameters passed to C<create>.

=back

=head2 bug_end_of_update

This happens at the end of L<Bugzilla::Bug/update>, after all other changes are
made to the database. This generally occurs inside a database transaction.

Params:

=over

=item C<bug> 

The changed bug object, with all fields set to their updated values.

=item C<old_bug>

A bug object pulled from the database before the fields were set to
their updated values (so it has the old values available for each field).

=item C<timestamp> 

The timestamp used for all updates in this transaction, as a SQL date
string.

=item C<changes> 

The hash of changed fields. C<< $changes->{field} = [old, new] >>

=back

=head2 bug_check_can_change_field

This hook controls what fields users are allowed to change. You can add code
here for site-specific policy changes and other customizations. 

This hook is only executed if the field's new and old values differ. 

Any denies take priority over any allows. So, if another extension denies
a change but yours allows the change, the other extension's deny will
override your extension's allow.

Params:

=over

=item C<bug>

L<Bugzilla::Bug> - The current bug object that this field is changing on.

=item C<field>

The name (from the C<fielddefs> table) of the field that we are checking.

=item C<new_value>

The new value that the field is being changed to.

=item C<old_value>

The old value that the field is being changed from.

=item C<priv_results>

C<array> - This is how you explicitly allow or deny a change. You should only
push something into this array if you want to explicitly allow or explicitly
deny the change, and thus skip all other permission checks that would otherwise
happen after this hook is called. If you don't care about the field change,
then don't push anything into the array.

The pushed value should be a choice from the following constants:

=over

=item C<PRIVILEGES_REQUIRED_NONE>

No privileges required. This explicitly B<allows> a change.

=item C<PRIVILEGES_REQUIRED_REPORTER>

User is not the reporter, assignee or an empowered user, so B<deny>.

=item C<PRIVILEGES_REQUIRED_ASSIGNEE>

User is not the assignee or an empowered user, so B<deny>.

=item C<PRIVILEGES_REQUIRED_EMPOWERED>

User is not a sufficiently empowered user, so B<deny>.

=back

=back

=head2 bug_fields

Allows the addition of database fields from the bugs table to the standard
list of allowable fields in a L<Bugzilla::Bug> object, so that
you can call the field as a method.

Note: You should add here the names of any fields you added in L</bug_columns>.

Params:

=over

=item C<columns> - A arrayref containing an array of column names. Push
your column name(s) onto the array.

=back

=head2 bug_format_comment

Allows you to do custom parsing on comments before they are displayed. You do
this by returning two regular expressions: one that matches the section you
want to replace, and then another that says what you want to replace that
match with.

The matching and replacement will be run with the C</g> switch on the regex.

Params:

=over

=item C<regexes>

An arrayref of hashrefs.

You should push a hashref containing two keys (C<match> and C<replace>)
in to this array. C<match> is the regular expression that matches the
text you want to replace, C<replace> is what you want to replace that
text with. (This gets passed into a regular expression like 
C<s/$match/$replace/>.)

Instead of specifying a regular expression for C<replace> you can also
return a coderef (a reference to a subroutine). If you want to use
backreferences (using C<$1>, C<$2>, etc. in your C<replace>), you have to use
this method--it won't work if you specify C<$1>, C<$2> in a regular expression
for C<replace>. Your subroutine will get a hashref as its only argument. This
hashref contains a single key, C<matches>. C<matches> is an arrayref that
contains C<$1>, C<$2>, C<$3>, etc. in order, up to C<$10>. Your subroutine
should return what you want to replace the full C<match> with. (See the code
example for this hook if you want to see how this actually all works in code.
It's simpler than it sounds.)

B<You are responsible for HTML-escaping your returned data.> Failing to
do so could open a security hole in Bugzilla.

=item C<text>

A B<reference> to the exact text that you are parsing.

Generally you should not modify this yourself. Instead you should be 
returning regular expressions using the C<regexes> array.

The text has not been parsed in any way. (So, for example, it is not
HTML-escaped. You get "&", not "&amp;".)

=item C<bug>

The L<Bugzilla::Bug> object that this comment is on. Sometimes this is
C<undef>, meaning that we are parsing text that is not on a bug.

=item C<comment>

A L<Bugzilla::Comment> object representing the comment you are about to
parse.

Sometimes this is C<undef>, meaning that we are parsing text that is
not a bug comment (but could still be some other part of a bug, like
the summary line).

=item C<user>

The L<Bugzilla::User> object representing the user who will see the text.
This is useful to determine how much confidential information can be displayed
to the user.

=back

=head2 bug_start_of_update

This happens near the beginning of L<Bugzilla::Bug/update>, after L<Bugzilla::Object/update>
is called, but before all other special changes are made to the database. Once use case is
this allows for adding your own entries to the C<changes> hash which gets added to the
bugs_activity table later keeping you from having to do it yourself. Also this is also helpful
if your extension needs to add CC members, flags, keywords, groups, etc. This generally
occurs inside a database transaction.

Params:

=over

=item C<bug> 

The changed bug object, with all fields set to their updated values.

=item C<old_bug>

A bug object pulled from the database before the fields were set to
their updated values (so it has the old values available for each field).

=item C<timestamp> 

The timestamp used for all updates in this transaction, as a SQL date
string.

=item C<changes> 

The hash of changed fields. C<< $changes->{field} = [old, new] >>

=back

=head2 bug_url_sub_classes

Allows you to add more L<Bugzilla::BugUrl> sub-classes.

See the C<MoreBugUrl> extension to see how things work.

Params:

=over

=item C<sub_classes> - An arrayref of strings which represent L<Bugzilla::BugUrl>
sub-classes.

=back

=head2 buglist_columns

This happens in L<Bugzilla::Search/COLUMNS>, which determines legal bug
list columns for F<buglist.cgi> and F<colchange.cgi>. It gives you the
opportunity to add additional display columns.

Params:

=over

=item C<columns> - A hashref, where the keys are unique string identifiers
for the column being defined and the values are hashrefs with the
following fields:

=over

=item C<name> - The name of the column in the database.

=item C<title> - The title of the column as displayed to users.

=back

The definition is structured as:

 $columns->{$id} = { name => $name, title => $title };

=back

=head2 buglist_column_joins

This allows you to join additional tables to display additional columns
in buglists. This hook is generally used in combination with the
C<buglist_columns> hook.

Params:

=over

=item C<column_joins> - A hashref containing data to return back to
L<Bugzilla::Search>. This hashref contains names of the columns as keys and 
a hashref about table to join as values. This hashref has the following keys:

=over

=item C<table> - The name of the additional table to join.

=item C<as> - (optional) The alias used for the additional table. This alias
must not conflict with an existing alias already used in the query.

=item C<from> - (optional) The name of the column in the C<bugs> table which
the additional table should be linked to. If omitted, C<bug_id> will be used.

=item C<to> - (optional) The name of the column in the additional table which
should be linked to the column in the C<bugs> table, see C<from> above.
If omitted, C<bug_id> will be used.

=item C<join> - (optional) Either INNER or LEFT. Determine how the additional
table should be joined with the C<bugs> table. If omitted, LEFT is used.

=back

=back

=head2 search_operator_field_override

This allows you to modify L<Bugzilla::Search/OPERATOR_FIELD_OVERRIDE>,
which determines the search functions for fields. It allows you to specify
custom search functionality for certain fields. 

See L<Bugzilla::Search/OPERATOR_FIELD_OVERRIDE> for reference and see
the code in the example extension.

Note that the interface to this hook is B<UNSTABLE> and it may change in the
future.

Params:

=over

=item C<operators> - See L<Bugzilla::Search/OPERATOR_FIELD_OVERRIDE> to get
an idea of the structure.

=item C<search> - The L<Bugzilla::Search> object.

=back

=head2 bugmail_recipients

This allows you to modify the list of users who are going to be receiving
a particular bugmail. It also allows you to specify why they are receiving
the bugmail.

Users' bugmail preferences will be applied to any users that you add
to the list. (So, for example, if you add somebody as though they were
a CC on the bug, and their preferences state that they don't get email
when they are a CC, they won't get email.)

This hook is called before watchers or globalwatchers are added to the
recipient list.

Params:

=over

=item C<bug>

The L<Bugzilla::Bug> that bugmail is being sent about.

=item C<recipients>

This is a hashref. The keys are numeric user ids from the C<profiles>
table in the database, for each user who should be receiving this bugmail.
The values are hashrefs. The keys in I<these> hashrefs correspond to
the "relationship" that the user has to the bug they're being emailed
about, and the value should always be C<1>. The "relationships"
are described by the various C<REL_> constants in L<Bugzilla::Constants>.

Here's an example of adding userid C<123> to the recipient list
as though they were on the CC list:

 $recipients->{123}->{+REL_CC} = 1

(We use C<+> in front of C<REL_CC> so that Perl interprets it as a constant
instead of as a string.)

=item C<users>

This is a hash of L<Bugzilla::User> objects, keyed by id. This is so you can
find out more information about any of the user ids in the C<recipients> hash.
Every id in the incoming C<recipients> hash will have an object in here. 
(But if you add additional recipients to the C<recipients> hash, you are 
B<not> required to add them to this hash.)

=item C<diffs>

This is a list of hashes, each hash representing a change to the bug. Each 
hash has the following members: C<field_name>, C<bug_when>, C<old>, C<new> 
and C<who> (a L<Bugzilla::User>). If appropriate, there will also be 
C<attach_id> or C<comment_id>; if either is present, there will be 
C<isprivate>. See C<_get_diffs> in F<Bugzilla/BugMail.pm> to see exactly how 
it is populated. Warning: the format and existence of the "diffs" parameter 
is subject to change in future releases of Bugzilla.

=back

=head2 bugmail_relationships

There are various sorts of "relationships" that a user can have to a bug,
such as Assignee, CC, etc. If you want to add a new type of relationship,
you should use this hook.

Params:

=over

=item C<relationships>

A hashref, where the keys are numbers and the values are strings.

The keys represent a numeric identifier for the relationship. The
numeric identifier should be a negative number between -1 and -127.
The number must be unique across all extensions. (Negative numbers
are used so as not to conflict with relationship identifiers in Bugzilla
itself.)

The value is the "name" of this relationship that will show up in email
headers in bugmails. The "name" should be short and should contain no
spaces.

=back


=head2 cgi_headers

This allows you to modify the HTTP headers sent out on every Bugzilla
response.

Params:

=over

=item C<headers>

A hashref, where the keys are header names and the values are header
values. Keys need to be lower-case, and begin with a "-". If you use
the "_" character it will be converted to "-", and the library will
also fix the casing to Camel-Case.

You can delete (some) headers that Bugzilla adds by deleting entries
from the hash.

=item C<cgi>

The CGI object, which may tell you useful things about the response on
which to base a decision of whether or not to add a header.

=back


=head2 config_add_panels

If you want to add new panels to the Parameters administrative interface,
this is where you do it.

Params:

=over

=item C<panel_modules>

A hashref, where the keys are the "name" of the panel and the value
is the Perl module representing that panel. For example, if
the name is C<Auth>, the value would be C<Bugzilla::Config::Auth>.

For your extension, the Perl module would start with 
C<Bugzilla::Extension::Foo>, where "Foo" is the name of your Extension.
(See the code in the example extension.)

=back

=head2 config_modify_panels

This is how you modify already-existing panels in the Parameters
administrative interface. For example, if you wanted to add a new
Auth method (modifying Bugzilla::Config::Auth) this is how you'd
do it.

Params:

=over

=item C<panels>

A hashref, where the keys are lower-case panel "names" (like C<auth>, 
C<admin>, etc.) and the values are hashrefs. The hashref contains a
single key, C<params>. C<params> is an arrayref--the return value from
C<get_param_list> for that module. You can modify C<params> and
your changes will be reflected in the interface.

Adding new keys to C<panels> will have no effect. You should use
L</config_add_panels> if you want to add new panels.

=back

=head2 email_in_before_parse

This happens right after an inbound email is converted into an Email::MIME
object, but before we start parsing the email to extract field data. This
means the email has already been decoded for you. It gives you a chance
to interact with the email itself before L<email_in> starts parsing its content.

=over

=item C<mail> - An Email::MIME object. The decoded incoming email.

=item C<fields> - A hashref. The hash which will contain extracted data.

=back

=head2 email_in_after_parse

This happens after all the data has been extracted from the email, but before
the reporter is validated, during L<email_in>. This lets you do things after
the normal parsing of the email, such as sanitizing field data, changing the
user account being used to file a bug, etc.

=over

=item C<fields> - A hashref. The hash containing the extracted field data.

=back

=head2 enter_bug_entrydefaultvars

B<DEPRECATED> - Use L</template_before_process> instead.

This happens right before the template is loaded on enter_bug.cgi.

Params:

=over

=item C<vars> - A hashref. The variables that will be passed into the template.

=back

=head2 error_catch

This hook allows extensions to catch errors thrown by Bugzilla and
take the appropriate actions.

Params:

=over

=item C<error>

A string representing the error code thrown by Bugzilla. This string
matches the C<error> variable in C<global/user-error.html.tmpl> and
C<global/code-error.html.tmpl>.

=item C<message>

If the error mode is set to C<ERROR_MODE_WEBPAGE>, you get a reference to
the whole HTML page with the error message in it, including its header and
footer. If you need to extract the error message itself, you can do it by
looking at the content of the table cell whose ID is C<error_msg>.
If the error mode is not set to C<ERROR_MODE_WEBPAGE>, you get a reference
to the error message itself.

=item C<vars>

This hash contains all the data passed to the error template. Its content
depends on the error thrown.

=back

=head2 flag_end_of_update

This happens at the end of L<Bugzilla::Flag/update_flags>, after all other
changes are made to the database and after emails are sent. It gives you a
before/after snapshot of flags so you can react to specific flag changes.
This generally occurs inside a database transaction.

Note that the interface to this hook is B<UNSTABLE> and it may change in the
future.

Params:

=over

=item C<object> - The changed bug or attachment object.

=item C<timestamp> - The timestamp used for all updates in this transaction,
as a SQL date string.

=item C<old_flags> - The snapshot of flag summaries from before the change.

=item C<new_flags> - The snapshot of flag summaries after the change. Call
C<my ($removed, $added) = diff_arrays(old_flags, new_flags)> to get the list of
changed flags, and search for a specific condition like C<added eq 'review-'>.

=back

=head2 group_before_delete

This happens in L<Bugzilla::Group/remove_from_db>, after we've confirmed
that the group can be deleted, but before any rows have actually
been removed from the database. This occurs inside a database
transaction.

Params:

=over

=item C<group> - The L<Bugzilla::Group> being deleted.

=back

=head2 group_end_of_create

This happens at the end of L<Bugzilla::Group/create>, after all other
changes are made to the database. This occurs inside a database transaction.

Params:

=over

=item C<group> 

The new L<Bugzilla::Group> object that was just created.

=back

=head2 group_end_of_update

This happens at the end of L<Bugzilla::Group/update>, after all other 
changes are made to the database. This occurs inside a database transaction.

Params:

=over

=item C<group> - The changed L<Bugzilla::Group> object, with all fields set 
to their updated values.

=item C<changes> - The hash of changed fields. 
C<< $changes->{$field} = [$old, $new] >>

=back

=head2 install_before_final_checks

Allows execution of custom code before the final checks are done in 
checksetup.pl.

Params:

=over

=item C<silent>

A flag that indicates whether or not checksetup is running in silent mode.
If this is true, messages that are I<always> printed by checksetup.pl should be
suppressed, but messages about any changes that are just being done this one
time should be printed.

=back

=head2 install_filesystem

Allows for additional files and directories to be added to the
list of files and directories already managed by checksetup.pl.
You will be able to also set permissions for the files and
directories using this hook. You can also use this hook to create 
appropriate .htaccess files for any directory to secure its contents.
For examples see  L<FILESYSTEM> in L<Bugzilla::Install::Filesystem>.

Params:

=over

=item C<files>

Hash reference of files that are already present when your extension was
installed but need to have specific permissions set. Each file key
points to another hash reference containing the following settings.

Params:

=over

=item C<perms> - Permissions to be set on the file.

=back

=item C<create_dirs>

Hash reference containing the name of each directory that will be created,
pointing at its default permissions.

=item C<non_recurse_dirs>

Hash reference containing directories that we want to set the perms on, but not
recurse through. These are directories not created in checksetup.pl. Each directory
key's value is the permissions to be set on the directory.

=item C<recurse_dirs>

Hash reference of directories that will have permissions set for each item inside 
each of the directories, including the directory itself. Each directory key
points to another hash reference containing the following settings.

Params:

=over

=item C<files> - Permissions to be set on any files beneath the directory.

=item C<dirs> - Permissions to be set on the directory itself and any directories
beneath it.

=back

=item C<create_files>

Hash reference of additional files to be created. Each file key points to another
hash reference containing the following settings.

Params:

=over

=item C<perms> - The permissions to be set on the file itself.

=item C<contents> - The contents to be added to the file or leave blank for an
empty file.

=back

=item C<htaccess>

Hash reference containing htaccess files to be created. You can set the permissions
for the htaccess as well as the contents of the file. Each file key points to another 
hash reference containing the following settings.

Params:

=over

=item C<perms> - Permissions to be set on the htaccess file.

=item C<contents> - Contents of the htaccess file. It can be set manually or
use L<HT_DEFAULT_DENY> defined in L<Bugzilla::Install::Filesystem> to deny all
by default.

=back

=back

=head2 install_update_db

This happens at the very end of all the tables being updated
during an installation or upgrade. If you need to modify your custom
schema or add new columns to existing tables, do it here. No params are
passed.

=head2 install_update_db_fielddefs

This is used to update the schema of the fielddefs table before
any other schema changes take place. No params are passed.

This hook should only be used for updating the schema of the C<fielddefs>
table. Do not modify any other table in this hook. To modify other tables, use
the L</install_update_db> hook.

=head2 db_schema_abstract_schema

This allows you to add tables to Bugzilla. Note that we recommend that you 
prefix the names of your tables with some word (preferably the name of
your Extension), so that they don't conflict with any future Bugzilla tables.

If you wish to add new I<columns> to existing Bugzilla tables, do that
in L</install_update_db>.

Params:

=over

=item C<schema> - A hashref, in the format of 
L<Bugzilla::DB::Schema/ABSTRACT_SCHEMA>. Add new hash keys to make new table
definitions. F<checksetup.pl> will automatically add these tables to the
database when run.

=back

=head2 job_map

Bugzilla has a system - L<Bugzilla::JobQueue> - for running jobs 
asynchronously, if the administrator has set it up. This hook allows the 
addition of mappings from job names to handler classes, so an extension can 
fire off jobs.

Params:

=over

=item C<job_map> - The job map hash. Key: the name of the job, as should be 
passed to Bugzilla->job_queue->insert(). Value: the name of the Perl module 
which implements the task (an instance of L<TheSchwartz::Worker>). 

=back

=head2 mailer_before_send

Called right before L<Bugzilla::Mailer> sends a message to the MTA.

Params:

=over

=item C<email> - The C<Email::MIME> object that's about to be sent.

=back

=head2 object_before_create

This happens at the beginning of L<Bugzilla::Object/create>.

Params:

=over

=item C<class>

The name of the class that C<create> was called on. You can check this 
like C<< if ($class->isa('Some::Class')) >> in your code, to perform specific
tasks before C<create> for only certain classes.

=item C<params>

A hashref. The set of named parameters passed to C<create>.

=back


=head2 object_before_delete

This happens in L<Bugzilla::Object/remove_from_db>, after we've confirmed
that the object can be deleted, but before any rows have actually
been removed from the database. This sometimes occurs inside a database
transaction.

Params:

=over

=item C<object> - The L<Bugzilla::Object> being deleted. You will probably
want to check its type like C<< $object->isa('Some::Class') >> before doing
anything with it.

=back


=head2 object_before_set

Called during L<Bugzilla::Object/set>, before any actual work is done.
You can use this to perform actions before a value is changed for
specific fields on certain types of objects.

Params:

=over

=item C<object>

The object that C<set> was called on. You will probably want to
do something like C<< if ($object->isa('Some::Class')) >> in your code to
limit your changes to only certain subclasses of Bugzilla::Object.

=item C<field>

The name of the field being updated in the object.

=item C<value> 

The value being set on the object.

=back

=head2 object_columns

This hook allows you to add new "fields" to existing Bugzilla objects,
that correspond to columns in their tables.

For example, if you added an C<example> column to the "bugs" table, you
would have to also add an C<example> field to the C<Bugzilla::Bug> object
in order to access that data via Bug objects.

Don't do anything slow inside this hook--it's called several times on
every page of Bugzilla.

Params:

=over

=item C<class>

The name of the class that this hook is being called on. You can check this 
like C<< if ($class->isa('Some::Class')) >> in your code, to add new
fields only for certain classes.

=item C<columns>

An arrayref. Add the string names of columns to this array to add new
values to objects. 

For example, if you add an C<example> column to a particular table
(using L</install_update_db>), and then push the string C<example> into 
this array for the object that uses that table, then you can access the
information in that column via C<< $object->{example} >> on all objects
of that type.

This arrayref does not contain the standard column names--you cannot modify
or remove standard object columns using this hook.

=back

=head2 object_end_of_create

Called at the end of L<Bugzilla::Object/create>, after all other changes are
made to the database. This occurs inside a database transaction.

Params:

=over

=item C<class>

The name of the class that C<create> was called on. You can check this 
like C<< if ($class->isa('Some::Class')) >> in your code, to perform specific
tasks for only certain classes.

=item C<object>

The created object.

=back

=head2 object_end_of_create_validators

Called at the end of L<Bugzilla::Object/run_create_validators>. You can
use this to run additional validation when creating an object.

If a subclass has overridden C<run_create_validators>, then this usually
happens I<before> the subclass does its custom validation.

Params:

=over

=item C<class>

The name of the class that C<create> was called on. You can check this 
like C<< if ($class->isa('Some::Class')) >> in your code, to perform specific
tasks for only certain classes.

=item C<params>

A hashref. The set of named parameters passed to C<create>, modified and
validated by the C<VALIDATORS> specified for the object.

=back


=head2 object_end_of_set

Called during L<Bugzilla::Object/set>, after all the code of the function
has completed (so the value has been validated and the field has been set
to the new value). You can use this to perform actions after a value is
changed for specific fields on certain types of objects.

The new value is not specifically passed to this hook because you can
get it as C<< $object->{$field} >>.

Params:

=over

=item C<object>

The object that C<set> was called on. You will probably want to
do something like C<< if ($object->isa('Some::Class')) >> in your code to
limit your changes to only certain subclasses of Bugzilla::Object.

=item C<field>

The name of the field that was updated in the object.

=back


=head2 object_end_of_set_all

This happens at the end of L<Bugzilla::Object/set_all>. This is a
good place to call custom set_ functions on objects, or to make changes
to an object before C<update()> is called.

Params:

=over

=item C<object>

The L<Bugzilla::Object> which is being updated. You will probably want to
do something like C<< if ($object->isa('Some::Class')) >> in your code to
limit your changes to only certain subclasses of Bugzilla::Object.

=item C<params>

A hashref. The set of named parameters passed to C<set_all>.

=back

=head2 object_end_of_update

Called during L<Bugzilla::Object/update>, after changes are made
to the database, but while still inside a transaction.

Params:

=over

=item C<object>

The object that C<update> was called on. You will probably want to
do something like C<< if ($object->isa('Some::Class')) >> in your code to
limit your changes to only certain subclasses of Bugzilla::Object.

=item C<old_object>

The object as it was before it was updated.

=item C<changes>

The fields that have been changed, in the same format that
L<Bugzilla::Object/update> returns.

=back

=head2 object_update_columns

If you've added fields to bugs via L</object_columns>, then this
hook allows you to say which of those columns should be updated in the
database when L<Bugzilla::Object/update> is called on the object.

If you don't use this hook, then your custom columns won't be modified in
the database by Bugzilla.

Params:

=over

=item C<object>

The object that is about to be updated. You should check this
like C<< if ($object->isa('Some::Class')) >> in your code, to modify
the "update columns" only for certain classes.

=item C<columns>

An arrayref. Add the string names of columns to this array to allow
that column to be updated when C<update()> is called on the object.

This arrayref does not contain the standard column names--you cannot stop
standard columns from being updated by using this hook.

=back

=head2 object_validators

Allows you to add new items to L<Bugzilla::Object/VALIDATORS> for
particular classes.

Params:

=over

=item C<class>

The name of the class that C<VALIDATORS> was called on. You can check this 
like C<< if ($class->isa('Some::Class')) >> in your code, to add
validators only for certain classes.

=item C<validators>

A hashref, where the keys are database columns and the values are subroutine
references. You can add new validators or modify existing ones. If you modify
an existing one, you should remember to call the original validator
inside of your modified validator. (This way, several extensions can all
modify the same validator.)

=back


=head2 page_before_template

This is a simple way to add your own pages to Bugzilla. This hooks C<page.cgi>,
which loads templates from F<template/en/default/pages>. For example,
C<page.cgi?id=fields.html> loads F<template/en/default/pages/fields.html.tmpl>.

This hook is called right before the template is loaded, so that you can
pass your own variables to your own pages.

You can also use this to implement complex custom pages, by doing your own
output and then calling  C<exit> at the end of the hook, thus preventing
the normal C<page.cgi> behavior from occurring.

Params:

=over

=item C<page_id>

This is the name of the page being loaded, like C<fields.html>.

Note that if two extensions use the same name, it is uncertain which will
override the others, so you should be careful with how you name your pages.
Usually extensions prefix their pages with a directory named after their
extension, so for an extension named "Foo", page ids usually look like
C<foo/mypage.html>.

=item C<vars>

This is a hashref--put variables into here if you want them passed to
your template.

=back

=head2 path_info_whitelist

By default, Bugzilla removes the Path-Info information from URLs before
passing data to CGI scripts. If this information is needed for your
customizations, you can enumerate the pages you want to whitelist here.

Params:

=over

=item C<whitelist>

An array of script names that will not have their Path-Info automatically
removed.

=back

=head2 post_bug_after_creation

B<DEPRECATED> (Use L</bug_end_of_create> instead.)

This happens after a bug is created and before bug mail is sent
during C<post_bug.cgi>. Note that this only happens during C<post_bug.cgi>,
it doesn't happen during any of the other methods of creating a bug.

Params:

=over

=item C<vars> - The template vars hashref.

=back


=head2 product_confirm_delete

B<DEPRECATED> - Use L</template_before_process> instead.

Called before displaying the confirmation message when deleting a product.

Params:

=over

=item C<vars> - The template vars hashref.

=back

=head2 product_end_of_create

Called right after a new product has been created, allowing additional
changes to be made to the new product's attributes. This occurs inside of
a database transaction, so if the hook throws an error, all previous
changes will be rolled back, including the creation of the new product.
(However, note that such rollbacks should not normally be used, as
some databases that Bugzilla supports have very bad rollback performance.
If you want to validate input and throw errors before the Product is created,
use L</object_end_of_create_validators> instead, or add a validator 
using L</object_validators>.)

Params:

=over

=item C<product> - The new L<Bugzilla::Product> object that was just created.

=back

=head2 quicksearch_map

This hook allows you to alter the Quicksearch syntax to include e.g. special 
searches for custom fields you have.

Params:

=over

=item C<map> - a hash where the key is the name you want to use in 
Quicksearch, and the value is the name from the C<fielddefs> table that you 
want it to map to. You can modify existing mappings or add new ones.

=back

=head2 sanitycheck_check

This hook allows for extra sanity checks to be added, for use by
F<sanitycheck.cgi>.

Params:

=over

=item C<status> - a CODEREF that allows status messages to be displayed
to the user. (F<sanitycheck.cgi>'s C<Status>)

=back

=head2 sanitycheck_repair

This hook allows for extra sanity check repairs to be made, for use by
F<sanitycheck.cgi>.

Params:

=over

=item C<status> - a CODEREF that allows status messages to be displayed
to the user. (F<sanitycheck.cgi>'s C<Status>)

=back

=head2 template_before_create

This hook allows you to modify the configuration of L<Bugzilla::Template>
objects before they are created. For example, you could add a new
global template variable this way.

Params:

=over

=item C<config>

A hashref--the configuration that will be passed to L<Template/new>.
See L<http://template-toolkit.org/docs/modules/Template.html#section_CONFIGURATION_SUMMARY>
for information on how this configuration variable is structured (or just
look at the code for C<create> in L<Bugzilla::Template>.)

=back

=head2 template_before_process

This hook is called any time Bugzilla processes a template file, including
calls to C<< $template->process >>, C<PROCESS> statements in templates,
and C<INCLUDE> statements in templates. It is not called when templates
process a C<BLOCK>, only when they process a file.

This hook allows you to define additional variables that will be available to
the template being processed, or to modify the variables that are currently
in the template. It works exactly as though you inserted code to modify
template variables at the top of a template.

You probably want to restrict this hook to operating only if a certain 
file is being processed (which is why you get a C<file> argument
below). Otherwise, modifying the C<vars> argument will affect every single
template in Bugzilla.

B<Note:> This hook is not called if you are already in this hook.
(That is, it won't call itself recursively.) This prevents infinite
recursion in situations where this hook needs to process a template
(such as if this hook throws an error).

Params:

=over

=item C<vars>

This is the entire set of variables that the current template can see.
Technically, this is a L<Template::Stash> object, but you can just
use it like a hashref if you want.

=item C<file>

The name of the template file being processed. This is relative to the
main template directory for the language (i.e. for
F<template/en/default/bug/show.html.tmpl>, this variable will contain
C<bug/show.html.tmpl>).

=item C<context>

A L<Template::Context> object. Usually you will not have to use this, but
if you need information about the template itself (other than just its
name), you can get it from here.

=back

=head2 user_check_account_creation

This hook permits you to do extra checks before the creation of a new user
account. This hook is called after email address validation has been done.
Note that this hook can also access the IP address of the requester thanks
to the C<remote_ip()> subroutine exported by C<Bugzilla::Util>.

Params:

=over

=item C<login>

The login of the new account. This is usually an email address, unless the
C<emailsuffix> parameter is not empty.

=back

=head2 user_preferences

This hook allows you to add additional panels to the User Preferences page,
and validate data displayed and returned from these panels. It works in
combination with the C<tabs> hook available in the
F<template/en/default/account/prefs/prefs.html.tmpl> template. To make it
work, you must define two templates in your extension:
F<extensions/Foo/template/en/default/hook/account/prefs/prefs-tabs.html.tmpl>
contains a list of additional panels to include.
F<extensions/Foo/template/en/default/account/prefs/bar.html.tmpl> contains
the content of the panel itself. See the C<Example> extension to see how
things work.

Params:

=over

=item C<current_tab>

The name of the current panel being viewed by the user. You should always
make sure that the name of the panel matches what you expect it to be.
Else you could be interacting with the panel of another extension.

=item C<save_changes>

A boolean which is true when data should be validated and the DB updated
accordingly. This means the user clicked the "Submit Changes" button.

=item C<handled>

This is a B<reference> to a scalar, not a scalar. (So you would set it like
C<$$handled = 1>, not like C<$handled = 1>.) Set this to a true value to let
Bugzilla know that the passed-in panel is valid and that you have handled it.
(Otherwise, Bugzilla will throw an error that the panel is invalid.) Don't set
this to true if you didn't handle the panel listed in C<current_tab>.

=item C<vars>

You can add as many new key/value pairs as you want to this hashref.
It will be passed to the template.

=back

=head2 webservice

This hook allows you to add your own modules to the WebService. (See
L<Bugzilla::WebService>.)

Params:

=over

=item C<dispatch>

A hashref where you can specify the names of your modules and which Perl
module handles the functions for that module. (This is actually sent to 
L<SOAP::Lite/dispatch_with>. You can see how that's used in F<xmlrpc.cgi>.)

The Perl module name will most likely start with C<Bugzilla::Extension::Foo::>
(where "Foo" is the name of your extension).

Example:

  $dispatch->{'Example.Blah'} = "Bugzilla::Extension::Example::Webservice::Blah";

And then you'd have a module F<extensions/Example/lib/Webservice/Blah.pm>,
and could call methods from that module like C<Example.Blah.Foo()> using
the WebServices interface.

It's recommended that all the keys you put in C<dispatch> start with the
name of your extension, so that you don't conflict with the standard Bugzilla
WebService functions (and so that you also don't conflict with other
plugins).

=back

=head2 webservice_error_codes

If your webservice extension throws custom errors, you can set numeric
codes for those errors here.

Extensions should use error codes above 10000, unless they are re-using
an already-existing error code.

Params:

=over

=item C<error_map>

A hash that maps the names of errors (like C<invalid_param>) to numbers.
See L<Bugzilla::WebService::Constants/WS_ERROR_CODE> for an example.

=back

=head2 webservice_fix_credentials

This hook allows for altering the credential parameters provided by the client
before authentication actually occurs. For example, this can be used to allow mapping
of custom parameters to the standard Bugzilla_login and Bugzilla_password parameters.

Params:

=over

=item C<params>

A hash ref containing the parameters passed into the webservice after
they have been obtained from the URL or body of the request.

=back

=head2 webservice_rest_request

This hook allows for altering any of the parameters provided by the client
after authentication has occured. You are able to change things like renaming
of keys, removing values, or adding additional information.

Params:

=over

=item C<params>

A hash ref containing the parameters passed into the webservice after
they have been obtained from the URL or body of the request.

=item C<rpc>

The current JSONRPC, XMLRPC, or REST object.

=back

=head2 webservice_rest_resources

This hook allows for altering of the REST resources data allowing you to
add additional paths to perform additional operations or to override the
resources already provided by the webservice modules.

Params:

=over

=item C<resources>

A hash returned from each module loaded that is used to determine
which code handler to use based on a regex match of the CGI path.

=item C<rpc>

The current JSONRPC, XMLRPC, or REST object.

=back

=head2 webservice_rest_response

This hook allows for altering the result data or response object
that is being returned by the current REST webservice call.

Params:

=over

=item C<response>

The HTTP response object generated by JSON-RPC library. You can use this
to add headers, etc.

=item C<result>

A reference to a hash that contains the result data.

=item C<rpc>

The current JSONRPC, XMLRPC, or REST object.

=back

=head2 webservice_status_code_map

This hook allows an extension to change the status codes returned by
specific webservice errors. The valid internal error codes that Bugzilla
generates, and the status codes they map to by default, are defined in the
C<WS_ERROR_CODE> constant in C<Bugzilla::WebService::Constants>. When
remapping an error, you may wish to use an existing status code constant.
Such constants are also in C<Bugzilla::WebService::Constants> and start
with C<STATUS_*> such as C<STATUS_BAD_REQUEST>.

Params:

=over

=item C<status_code_map>

A hash reference containing the current status code mapping.

=back

=head1 SEE ALSO

L<Bugzilla::Extension>
