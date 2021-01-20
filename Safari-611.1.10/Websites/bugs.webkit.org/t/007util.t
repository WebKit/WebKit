# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

#################
#Bugzilla Test 7#
#####Util.pm#####

use 5.10.1;
use strict;
use warnings;

use lib 't';
use Support::Files;
use Test::More tests => 17;
use DateTime;

BEGIN { 
    use_ok('Bugzilla');
    use_ok('Bugzilla::Util');
}

# We need to override user preferences so we can get an expected value when
# Bugzilla::Util::format_time() calls ask for the 'timezone' user preference.
Bugzilla->user->{'settings'}->{'timezone'}->{'value'} = "local";

# We need to know the local timezone for the date chosen in our tests.
# Below, tests are run against Nov. 24, 2002.
my $tz = Bugzilla->local_timezone->short_name_for_datetime(DateTime->new(year => 2002, month => 11, day => 24));

# we don't test the taint functions since that's going to take some more work.
# XXX: test taint functions

#html_quote():
is(html_quote("<lala&@>"),"&lt;lala&amp;&#64;&gt;",'html_quote');

#url_quote():
is(url_quote("<lala&>gaa\"'[]{\\"),"%3Clala%26%3Egaa%22%27%5B%5D%7B%5C",'url_quote');

#trim():
is(trim(" fg<*\$%>+=~~ "),'fg<*$%>+=~~','trim()');

#format_time();
is(format_time("2002.11.24 00:05"), "2002-11-24 00:05 $tz",'format_time("2002.11.24 00:05") is ' . format_time("2002.11.24 00:05"));
is(format_time("2002.11.24 00:05:56"), "2002-11-24 00:05:56 $tz",'format_time("2002.11.24 00:05:56")');
is(format_time("2002.11.24 00:05:56", "%Y-%m-%d %R"), '2002-11-24 00:05', 'format_time("2002.11.24 00:05:56", "%Y-%m-%d %R") (with no timezone)');
is(format_time("2002.11.24 00:05:56", "%Y-%m-%d %R %Z"), "2002-11-24 00:05 $tz", 'format_time("2002.11.24 00:05:56", "%Y-%m-%d %R %Z") (with timezone)');

# email_filter
my %email_strings = (
    'somebody@somewhere.com' => 'somebody',
    'Somebody <somebody@somewhere.com>' => 'Somebody <somebody>',
    'One Person <one@person.com>, Two Person <two@person.com>' 
        => 'One Person <one>, Two Person <two>',
    'This string contains somebody@somewhere.com and also this@that.com'
        => 'This string contains somebody and also this',
);
foreach my $input (keys %email_strings) {
    is(Bugzilla::Util::email_filter($input), $email_strings{$input}, 
       "email_filter('$input')");
}

# validate_email_syntax. We need to override some parameters.
my $params = Bugzilla->params;
$params->{emailregexp} = '.*';
$params->{emailsuffix} = '';
my $ascii_email = 'admin@company.com';
# U+0430 returns the Cyrillic "а", which looks similar to the ASCII "a".
my $utf8_email = "\N{U+0430}dmin\@company.com";
ok(validate_email_syntax($ascii_email), 'correctly formatted ASCII-only email address is valid');
ok(!validate_email_syntax($utf8_email), 'correctly formatted email address with non-ASCII characters is rejected');

# diff_arrays():
my @old_array = qw(alpha beta alpha gamma gamma beta alpha delta epsilon gamma);
my @new_array = qw(alpha alpha beta gamma epsilon delta beta delta);
# The order is not relevant when comparing both arrays for matching items,
# i.e. (foo bar) and (bar foo) are the same arrays (same items).
# But when returning data, we try to respect the initial order.
# We remove the leftmost items first, and return what's left. This means:
# Removed (in this order): gamma alpha gamma.
# Added (in this order): delta
my ($removed, $added) = diff_arrays(\@old_array, \@new_array);
is_deeply($removed, [qw(gamma alpha gamma)], 'diff_array(\@old, \@new) (check removal)');
is_deeply($added, [qw(delta)], 'diff_array(\@old, \@new) (check addition)');
