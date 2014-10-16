# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::WebService::Server;
use strict;

use Bugzilla::Error;
use Bugzilla::Util qw(datetime_from);

use Scalar::Util qw(blessed);

sub handle_login {
    my ($self, $class, $method, $full_method) = @_;
    eval "require $class";
    ThrowCodeError('unknown_method', {method => $full_method}) if $@;
    return if ($class->login_exempt($method) 
               and !defined Bugzilla->input_params->{Bugzilla_login});
    Bugzilla->login();
}

sub datetime_format_inbound {
    my ($self, $time) = @_;
    
    my $converted = datetime_from($time, Bugzilla->local_timezone);
    if (!defined $converted) {
        ThrowUserError('illegal_date', { date => $time });
    }
    $time = $converted->ymd() . ' ' . $converted->hms();
    return $time
}

sub datetime_format_outbound {
    my ($self, $date) = @_;

    return undef if (!defined $date or $date eq '');

    my $time = $date;
    if (blessed($date)) {
        # We expect this to mean we were sent a datetime object
        $time->set_time_zone('UTC');
    } else {
        # We always send our time in UTC, for consistency.
        # passed in value is likely a string, create a datetime object
        $time = datetime_from($date, 'UTC');
    }
    return $time->iso8601();
}

1;
