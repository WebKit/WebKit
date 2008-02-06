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
# The Original Code are the Bugzilla Tests.
# 
# The Initial Developer of the Original Code is Zach Lipton
# Portions created by Zach Lipton are 
# Copyright (C) 2002 Zach Lipton.  All
# Rights Reserved.
# 
# Contributor(s): Zach Lipton <zach@zachlipton.com>


#################
#Bugzilla Test 7#
#####Util.pm#####

use lib 't';
use Support::Files;

BEGIN { 
        use Test::More tests => 14;
        use_ok(Bugzilla::Util);
}

# We need to override the the Param() function so we can get an expected
# value when Bugzilla::Utim::format_time calls asks for Param('timezone').
# This will also prevent the tests from failing on site that do not have a
# data/params file containing Param('timezone') yet.
sub myParam {
    return "TEST" if $_[0] eq 'timezone';
}
*::Param = *myParam;

# we don't test the taint functions since that's going to take some more work.
# XXX: test taint functions

#html_quote():
is(html_quote("<lala&>"),"&lt;lala&amp;&gt;",'html_quote');

#url_quote():
is(url_quote("<lala&>gaa\"'[]{\\"),"%3Clala%26%3Egaa%22%27%5B%5D%7B%5C",'url_quote');

#value_quote():
is(value_quote("<lal\na&>g\naa\"'[\n]{\\"),"&lt;lal&#013;a&amp;&gt;g&#013;aa&quot;'[&#013;]{\\",'value_quote');

#lsearch():
my @list = ('apple','pear','plum','<"\\%');
is(lsearch(\@list,'pear'),1,'lsearch 1');
is(lsearch(\@list,'<"\\%'),3,'lsearch 2');
is(lsearch(\@list,'kiwi'),-1,'lsearch 3 (missing item)');

#max() and min():
@list = (7,27,636,2);
is(max(@list),636,'max()');
is(min(@list),2,'min()');

#trim():
is(trim(" fg<*\$%>+=~~ "),'fg<*$%>+=~~','trim()');

#format_time();
is(format_time("2002.11.24 00:05"),'2002-11-24 00:05 TEST','format_time("2002.11.24 00:05")');
is(format_time("2002.11.24 00:05:56"),'2002-11-24 00:05:56 TEST','format_time("2002.11.24 00:05:56")');
is(format_time("2002.11.24 00:05:56", "%Y-%m-%d %R"), '2002-11-24 00:05', 'format_time("2002.11.24 00:05:56", "%Y-%m-%d %R") (with no timezone)');
is(format_time("2002.11.24 00:05:56", "%Y-%m-%d %R %Z"), '2002-11-24 00:05 TEST', 'format_time("2002.11.24 00:05:56", "%Y-%m-%d %R %Z") (with timezone)');
