#!/usr/bin/perl -T
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
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::User::Setting;
use Bugzilla::Token;

my $template = Bugzilla->template;
my $user = Bugzilla->login(LOGIN_REQUIRED);
my $cgi = Bugzilla->cgi;
my $vars = {};

print $cgi->header;

$user->in_group('tweakparams')
  || ThrowUserError("auth_failure", {group  => "tweakparams",
                                     action => "modify",
                                     object => "settings"});

my $action = trim($cgi->param('action') || '');
my $token = $cgi->param('token');

if ($action eq 'update') {
    check_token_data($token, 'edit_settings');
    my $settings = Bugzilla::User::Setting::get_defaults();
    my $changed = 0;

    foreach my $name (keys %$settings) {
        my $old_enabled = $settings->{$name}->{'is_enabled'};
        my $old_value = $settings->{$name}->{'default_value'};
        my $enabled = defined $cgi->param("${name}-enabled") || 0;
        my $value = $cgi->param("${name}");
        my $setting = new Bugzilla::User::Setting($name);

        $setting->validate_value($value);

        if ($old_enabled != $enabled || $old_value ne $value) {
            Bugzilla::User::Setting::set_default($name, $value, $enabled);
            $changed = 1;
        }
    }
    $vars->{'message'} = 'default_settings_updated';
    $vars->{'changes_saved'} = $changed;
    Bugzilla->memcached->clear_config();
    delete_token($token);
}

# Don't use $settings as defaults may have changed.
$vars->{'settings'} = Bugzilla::User::Setting::get_defaults();
$vars->{'token'} = issue_session_token('edit_settings');

$template->process("admin/settings/edit.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
