#!/usr/bin/perl -wT
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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dave Miller <justdave@syndicomm.com>
#                 Joe Robins <jmrobins@tgix.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Shane H. W. Travis <travis@sedsystems.ca>

##############################################################################
#
# enter_bug.cgi
# -------------
# Displays bug entry form. Bug fields are specified through popup menus, 
# drop-down lists, or text fields. Default for these values can be 
# passed in as parameters to the cgi.
#
##############################################################################

use strict;

use lib qw(.);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Bug;
use Bugzilla::User;
require "CGI.pl";

use vars qw(
  $template
  $vars
  @enterable_products
  @legal_opsys
  @legal_platform
  @legal_priority
  @legal_severity
  @legal_keywords
  $userid
  %versions
  %target_milestone
  $proddesc
  $classdesc
);

# If we're using bug groups to restrict bug entry, we need to know who the 
# user is right from the start. 
Bugzilla->login(LOGIN_REQUIRED) if AnyEntryGroups();

my $cloned_bug;
my $cloned_bug_id;

my $cgi = Bugzilla->cgi;

my $product = $cgi->param('product');

if (!defined $product || $product eq "") {
    GetVersionTable();
    Bugzilla->login();

   if ( ! Param('useclassification') ) {
      # just pick the default one
      $cgi->param(-name => 'classification', -value => (keys %::classdesc)[0]);
   }

   if (!$cgi->param('classification')) {
       my %classdesc;
       my %classifications;
    
       foreach my $c (GetSelectableClassifications()) {
           my $found = 0;
           foreach my $p (@enterable_products) {
              if (CanEnterProduct($p)
                  && IsInClassification($c,$p)) {
                      $found = 1;
              }
           }
           if ($found) {
               $classdesc{$c} = $::classdesc{$c};
               $classifications{$c} = $::classifications{$c};
           }
       }

       my $classification_size = scalar(keys %classdesc);
       if ($classification_size == 0) {
           ThrowUserError("no_products");
       } 
       elsif ($classification_size > 1) {
           $vars->{'classdesc'} = \%classdesc;
           $vars->{'classifications'} = \%classifications;

           $vars->{'target'} = "enter_bug.cgi";
           $vars->{'format'} = $cgi->param('format');
           
           $vars->{'cloned_bug_id'} = $cgi->param('cloned_bug_id');

           print $cgi->header();
           $template->process("global/choose-classification.html.tmpl", $vars)
             || ThrowTemplateError($template->error());
           exit;        
       }
       $cgi->param(-name => 'classification', -value => (keys %classdesc)[0]);
   }

    my %products;
    foreach my $p (@enterable_products) {
        if (CanEnterProduct($p)) {
            if (IsInClassification(scalar $cgi->param('classification'),$p) ||
                $cgi->param('classification') eq "__all") {
                $products{$p} = $::proddesc{$p};
            }
        }
    }
 
    my $prodsize = scalar(keys %products);
    if ($prodsize == 0) {
        ThrowUserError("no_products");
    } 
    elsif ($prodsize > 1) {
        my %classifications;
        if ( ! Param('useclassification') ) {
            @{$classifications{"all"}} = keys %products;
        }
        elsif ($cgi->param('classification') eq "__all") {
            %classifications = %::classifications;
        } else {
            $classifications{$cgi->param('classification')} =
                $::classifications{$cgi->param('classification')};
        }
        $vars->{'proddesc'} = \%products;
        $vars->{'classifications'} = \%classifications;
        $vars->{'classdesc'} = \%::classdesc;

        $vars->{'target'} = "enter_bug.cgi";
        $vars->{'format'} = $cgi->param('format');

        $vars->{'cloned_bug_id'} = $cgi->param('cloned_bug_id');
        
        print $cgi->header();
        $template->process("global/choose-product.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;        
    } else {
        # Only one product exists
        $product = (keys %products)[0];
    }
}

##############################################################################
# Useful Subroutines
##############################################################################
sub formvalue {
    my ($name, $default) = (@_);
    return $cgi->param($name) || $default || "";
}

# Takes the name of a field and a list of possible values for that 
# field. Returns the first value in the list that is actually a 
# valid value for that field.
# The field should be named after its DB table.
# Returns undef if none of the platforms match.
sub pick_valid_field_value (@) {
    my ($field, @values) = @_;
    my $dbh = Bugzilla->dbh;

    foreach my $value (@values) {
        return $value if $dbh->selectrow_array(
            "SELECT 1 FROM $field WHERE value = ?", undef, $value); 
    }
    return undef;
}

sub pickplatform {
    return formvalue("rep_platform") if formvalue("rep_platform");

    my @platform;

    if (Param('defaultplatform')) {
        @platform = Param('defaultplatform');
    } else {
        # If @platform is a list, this function will return the first
        # item in the list that is a valid platform choice. If
        # no choice is valid, we return "Other".
        for ($ENV{'HTTP_USER_AGENT'}) {
        #PowerPC
            /\(.*PowerPC.*\)/i && do {@platform = "Macintosh"; last;};
            /\(.*PPC.*\)/ && do {@platform = "Macintosh"; last;};
            /\(.*AIX.*\)/ && do {@platform = "Macintosh"; last;};
        #Intel x86
            /\(.*[ix0-9]86.*\)/ && do {@platform = "PC"; last;};
        #Versions of Windows that only run on Intel x86
            /\(.*Win(?:dows )[39M].*\)/ && do {@platform = "PC"; last};
            /\(.*Win(?:dows )16.*\)/ && do {@platform = "PC"; last;};
        #Sparc
            /\(.*sparc.*\)/ && do {@platform = "Sun"; last;};
            /\(.*sun4.*\)/ && do {@platform = "Sun"; last;};
        #Alpha
            /\(.*AXP.*\)/i && do {@platform = "DEC"; last;};
            /\(.*[ _]Alpha.\D/i && do {@platform = "DEC"; last;};
            /\(.*[ _]Alpha\)/i && do {@platform = "DEC"; last;};
        #MIPS
            /\(.*IRIX.*\)/i && do {@platform = "SGI"; last;};
            /\(.*MIPS.*\)/i && do {@platform = "SGI"; last;};
        #68k
            /\(.*68K.*\)/ && do {@platform = "Macintosh"; last;};
            /\(.*680[x0]0.*\)/ && do {@platform = "Macintosh"; last;};
        #HP
            /\(.*9000.*\)/ && do {@platform = "HP"; last;};
        #ARM
#            /\(.*ARM.*\) && do {$platform = "ARM";};
        #Stereotypical and broken
            /\(.*Macintosh.*\)/ && do {@platform = "Macintosh"; last;};
            /\(.*Mac OS [89].*\)/ && do {@platform = "Macintosh"; last;};
            /\(Win.*\)/ && do {@platform = "PC"; last;};
            /\(.*Win(?:dows[ -])NT.*\)/ && do {@platform = "PC"; last;};
            /\(.*OSF.*\)/ && do {@platform = "DEC"; last;};
            /\(.*HP-?UX.*\)/i && do {@platform = "HP"; last;};
            /\(.*IRIX.*\)/i && do {@platform = "SGI"; last;};
            /\(.*(SunOS|Solaris).*\)/ && do {@platform = "Sun"; last;};
        #Braindead old browsers who didn't follow convention:
            /Amiga/ && do {@platform = "Macintosh"; last;};
            /WinMosaic/ && do {@platform = "PC"; last;};
        }
    }

    return pick_valid_field_value('rep_platform', @platform) || "Other";
}

sub pickos {
    if (formvalue('op_sys') ne "") {
        return formvalue('op_sys');
    }

    my @os;

    if (Param('defaultopsys')) {
        @os = Param('defaultopsys');
    } else {
        # This function will return the first
        # item in @os that is a valid platform choice. If
        # no choice is valid, we return "Other".
        for ($ENV{'HTTP_USER_AGENT'}) {
            /\(.*IRIX.*\)/ && do {@os = "IRIX"; last;};
            /\(.*OSF.*\)/ && do {@os = "OSF/1"; last;};
            /\(.*Linux.*\)/ && do {@os = "Linux"; last;};
            /\(.*Solaris.*\)/ && do {@os = "Solaris"; last;};
            /\(.*SunOS 5.*\)/ && do {@os = "Solaris"; last;};
            /\(.*SunOS.*sun4u.*\)/ && do {@os = "Solaris"; last;};
            /\(.*SunOS.*\)/ && do {@os = "SunOS"; last;};
            /\(.*HP-?UX.*\)/ && do {@os = "HP-UX"; last;};
            /\(.*BSD\/(?:OS|386).*\)/ && do {@os = "BSDI"; last;};
            /\(.*FreeBSD.*\)/ && do {@os = "FreeBSD"; last;};
            /\(.*OpenBSD.*\)/ && do {@os = "OpenBSD"; last;};
            /\(.*NetBSD.*\)/ && do {@os = "NetBSD"; last;};
            /\(.*BeOS.*\)/ && do {@os = "BeOS"; last;};
            /\(.*AIX.*\)/ && do {@os = "AIX"; last;};
            /\(.*OS\/2.*\)/ && do {@os = "OS/2"; last;};
            /\(.*QNX.*\)/ && do {@os = "Neutrino"; last;};
            /\(.*VMS.*\)/ && do {@os = "OpenVMS"; last;};
            /\(.*Windows XP.*\)/ && do {@os = "Windows XP"; last;};
            /\(.*Windows NT 5\.2.*\)/ && do {@os = "Windows Server 2003"; last;};
            /\(.*Windows NT 5\.1.*\)/ && do {@os = "Windows XP"; last;};
            /\(.*Windows 2000.*\)/ && do {@os = "Windows 2000"; last;};
            /\(.*Windows NT 5.*\)/ && do {@os = "Windows 2000"; last;};
            /\(.*Win.*9[8x].*4\.9.*\)/ && do {@os = "Windows ME"; last;};
            /\(.*Win(?:dows )M[Ee].*\)/ && do {@os = "Windows ME"; last;};
            /\(.*Win(?:dows )98.*\)/ && do {@os = "Windows 98"; last;};
            /\(.*Win(?:dows )95.*\)/ && do {@os = "Windows 95"; last;};
            /\(.*Win(?:dows )16.*\)/ && do {@os = "Windows 3.1"; last;};
            /\(.*Win(?:dows[ -])NT.*\)/ && do {@os = "Windows NT"; last;};
            /\(.*Windows.*NT.*\)/ && do {@os = "Windows NT"; last;};
            /\(.*32bit.*\)/ && do {@os = "Windows 95"; last;};
            /\(.*16bit.*\)/ && do {@os = "Windows 3.1"; last;};
            /\(.*Mac OS 9.*\)/ && do {@os = "Mac System 9.x"; last;};
            /\(.*Mac OS 8\.6.*\)/ && do {@os = "Mac System 8.6"; last;};
            /\(.*Mac OS 8\.5.*\)/ && do {@os = "Mac System 8.5"; last;};
        # Bugzilla doesn't have an entry for 8.1
            /\(.*Mac OS 8\.1.*\)/ && do {@os = "Mac System 8.0"; last;};
            /\(.*Mac OS 8\.0.*\)/ && do {@os = "Mac System 8.0"; last;};
            /\(.*Mac OS 8[^.].*\)/ && do {@os = "Mac System 8.0"; last;};
            /\(.*Mac OS 8.*\)/ && do {@os = "Mac System 8.6"; last;};
            /\(.*Mac OS X.*\)/ && do {@os = "Mac OS X 10.0"; last;};
            /\(.*Darwin.*\)/ && do {@os = "Mac OS X 10.0"; last;};
        # Silly
            /\(.*Mac.*PowerPC.*\)/ && do {@os = "Mac System 9.x"; last;};
            /\(.*Mac.*PPC.*\)/ && do {@os = "Mac System 9.x"; last;};
            /\(.*Mac.*68k.*\)/ && do {@os = "Mac System 8.0"; last;};
        # Evil
            /Amiga/i && do {@os = "Other"; last;};
            /WinMosaic/ && do {@os = "Windows 95"; last;};
            /\(.*PowerPC.*\)/ && do {@os = "Mac System 9.x"; last;};
            /\(.*PPC.*\)/ && do {@os = "Mac System 9.x"; last;};
            /\(.*68K.*\)/ && do {@os = "Mac System 8.0"; last;};
        }
    }

    push(@os, "Windows") if grep(/^Windows /, @os);
    push(@os, "Mac OS") if grep(/^Mac /, @os);

    return pick_valid_field_value('op_sys', @os) || "Other";
}
##############################################################################
# End of subroutines
##############################################################################

Bugzilla->login(LOGIN_REQUIRED) if (!(AnyEntryGroups()));

# If a user is trying to clone a bug
#   Check that the user has authorization to view the parent bug
#   Create an instance of Bug that holds the info from the parent
$cloned_bug_id = $cgi->param('cloned_bug_id');

if ($cloned_bug_id) {
    ValidateBugID($cloned_bug_id);
    $cloned_bug = new Bugzilla::Bug($cloned_bug_id, $userid);
}

# We need to check and make sure
# that the user has permission to enter a bug against this product.
CanEnterProductOrWarn($product);

GetVersionTable();

my $product_id = get_product_id($product);

if (1 == @{$::components{$product}}) {
    # Only one component; just pick it.
    $cgi->param('component', $::components{$product}->[0]);
}

my @components;

my $dbh = Bugzilla->dbh;
my $sth = $dbh->prepare(
       q{SELECT name, description, p1.login_name, p2.login_name 
           FROM components 
      LEFT JOIN profiles p1 ON components.initialowner = p1.userid
      LEFT JOIN profiles p2 ON components.initialqacontact = p2.userid
          WHERE product_id = ?
          ORDER BY name});

$sth->execute($product_id);
while (my ($name, $description, $owner, $qacontact)
       = $sth->fetchrow_array()) {
    push @components, {
        name => $name,
        description => $description,
        initialowner => $owner,
        initialqacontact => $qacontact || '',
    };
}

my %default;

$vars->{'product'}               = $product;
$vars->{'component_'}            = \@components;

$vars->{'priority'}              = \@legal_priority;
$vars->{'bug_severity'}          = \@legal_severity;
$vars->{'rep_platform'}          = \@legal_platform;
$vars->{'op_sys'}                = \@legal_opsys; 

$vars->{'use_keywords'}          = 1 if (@::legal_keywords);

$vars->{'assigned_to'}           = formvalue('assigned_to');
$vars->{'assigned_to_disabled'}  = !UserInGroup('editbugs');
$vars->{'cc_disabled'}           = 0;

$vars->{'qa_contact'}           = formvalue('qa_contact');
$vars->{'qa_contact_disabled'}  = !UserInGroup('editbugs');

$vars->{'cloned_bug_id'}         = $cloned_bug_id;

if ($cloned_bug_id) {

    $default{'component_'}    = $cloned_bug->{'component'};
    $default{'priority'}      = $cloned_bug->{'priority'};
    $default{'bug_severity'}  = $cloned_bug->{'bug_severity'};
    $default{'rep_platform'}  = $cloned_bug->{'rep_platform'};
    $default{'op_sys'}        = $cloned_bug->{'op_sys'};

    $vars->{'short_desc'}     = $cloned_bug->{'short_desc'};
    $vars->{'bug_file_loc'}   = $cloned_bug->{'bug_file_loc'};
    $vars->{'keywords'}       = $cloned_bug->keywords;
    $vars->{'dependson'}      = $cloned_bug_id;
    $vars->{'blocked'}        = "";
    $vars->{'deadline'}       = $cloned_bug->{'deadline'};

    if (defined $cloned_bug->cc) {
        $vars->{'cc'}         = join (" ", @{$cloned_bug->cc});
    } else {
        $vars->{'cc'}         = formvalue('cc');
    }

# We need to ensure that we respect the 'insider' status of
# the first comment, if it has one. Either way, make a note
# that this bug was cloned from another bug.

    $cloned_bug->longdescs();
    my $isprivate             = $cloned_bug->{'longdescs'}->[0]->{'isprivate'};

    $vars->{'comment'}        = "";
    $vars->{'commentprivacy'} = 0;

    if ( !($isprivate) ||
         ( ( Param("insidergroup") ) && 
           ( UserInGroup(Param("insidergroup")) ) ) 
       ) {
        $vars->{'comment'}        = $cloned_bug->{'longdescs'}->[0]->{'body'};
        $vars->{'commentprivacy'} = $isprivate;
    }

# Ensure that the groupset information is set up for later use.
    $cloned_bug->groups();

} # end of cloned bug entry form

else {

    $default{'component_'}    = formvalue('component');
    $default{'priority'}      = formvalue('priority', Param('defaultpriority'));
    $default{'bug_severity'}  = formvalue('bug_severity', Param('defaultseverity'));
    $default{'rep_platform'}  = pickplatform();
    $default{'op_sys'}        = pickos();

    $vars->{'short_desc'}     = formvalue('short_desc');
    $vars->{'bug_file_loc'}   = formvalue('bug_file_loc', "http://");
    $vars->{'keywords'}       = formvalue('keywords');
    $vars->{'dependson'}      = formvalue('dependson');
    $vars->{'blocked'}        = formvalue('blocked');
    $vars->{'deadline'}       = formvalue('deadline');

    $vars->{'cc'}             = join(', ', $cgi->param('cc'));

    $vars->{'comment'}        = formvalue('comment');
    $vars->{'commentprivacy'} = formvalue('commentprivacy');

} # end of normal/bookmarked entry form


# IF this is a cloned bug,
# AND the clone's product is the same as the parent's
#   THEN use the version from the parent bug
# ELSE IF a version is supplied in the URL
#   THEN use it
# ELSE IF there is a version in the cookie
#   THEN use it (Posting a bug sets a cookie for the current version.)
# ELSE
#   The default version is the last one in the list (which, it is
#   hoped, will be the most recent one).
#
# Eventually maybe each product should have a "current version"
# parameter.
$vars->{'version'} = $::versions{$product} || [];

if ( ($cloned_bug_id) &&
     ("$product" eq "$cloned_bug->{'product'}" ) ) {
    $default{'version'} = $cloned_bug->{'version'};
} elsif (formvalue('version')) {
    $default{'version'} = formvalue('version');
} elsif (defined $cgi->cookie("VERSION-$product") &&
    lsearch($vars->{'version'}, $cgi->cookie("VERSION-$product")) != -1) {
    $default{'version'} = $cgi->cookie("VERSION-$product");
} else {
    $default{'version'} = $vars->{'version'}->[$#{$vars->{'version'}}];
}

# Get list of milestones.
if ( Param('usetargetmilestone') ) {
    $vars->{'target_milestone'} = $::target_milestone{$product};
    if (formvalue('target_milestone')) {
       $default{'target_milestone'} = formvalue('target_milestone');
    } else {
       SendSQL("SELECT defaultmilestone FROM products WHERE " .
               "name = " . SqlQuote($product));
       $default{'target_milestone'} = FetchOneColumn();
    }
}


# List of status values for drop-down.
my @status;

# Construct the list of allowable values.  There are three cases:
# 
#  case                                 values
#  product does not have confirmation   NEW
#  confirmation, user cannot confirm    UNCONFIRMED
#  confirmation, user can confirm       NEW, UNCONFIRMED.

SendSQL("SELECT votestoconfirm FROM products WHERE name = " .
        SqlQuote($product));
if (FetchOneColumn()) {
    if (UserInGroup("editbugs") || UserInGroup("canconfirm")) {
        push(@status, "NEW");
    }
    push(@status, 'UNCONFIRMED');
} else {
    push(@status, "NEW");
}

$vars->{'bug_status'} = \@status; 

# Get the default from a template value if it is legitimate.
# Otherwise, set the default to the first item on the list.

if (formvalue('bug_status') && (lsearch(\@status, formvalue('bug_status')) >= 0)) {
    $default{'bug_status'} = formvalue('bug_status');
} else {
    $default{'bug_status'} = $status[0];
}
 
SendSQL("SELECT DISTINCT groups.id, groups.name, groups.description, " .
        "membercontrol, othercontrol " .
        "FROM groups LEFT JOIN group_control_map " .
        "ON group_id = id AND product_id = $product_id " .
        "WHERE isbuggroup != 0 AND isactive != 0 ORDER BY description");

my @groups;

while (MoreSQLData()) {
    my ($id, $groupname, $description, $membercontrol, $othercontrol) 
        = FetchSQLData();
    # Only include groups if the entering user will have an option.
    next if ((!$membercontrol) 
               || ($membercontrol == CONTROLMAPNA) 
               || ($membercontrol == CONTROLMAPMANDATORY)
               || (($othercontrol != CONTROLMAPSHOWN) 
                    && ($othercontrol != CONTROLMAPDEFAULT)
                    && (!UserInGroup($groupname)))
             );
    my $check;

    # If this is a cloned bug, 
    # AND the product for this bug is the same as for the original
    #   THEN set a group's checkbox if the original also had it on
    # ELSE IF this is a bookmarked template
    #   THEN set a group's checkbox if was set in the bookmark
    # ELSE
    #   set a groups's checkbox based on the group control map
    #
    if ( ($cloned_bug_id) &&
         ("$product" eq "$cloned_bug->{'product'}" ) ) {
        foreach my $i (0..(@{$cloned_bug->{'groups'}}-1) ) {
            if ($cloned_bug->{'groups'}->[$i]->{'bit'} == $id) {
                $check = $cloned_bug->{'groups'}->[$i]->{'ison'};
            }
        }
    }
    elsif(formvalue("maketemplate") ne "") {
        $check = formvalue("bit-$id", 0);
    }
    else {
        # Checkbox is checked by default if $control is a default state.
        $check = (($membercontrol == CONTROLMAPDEFAULT)
                 || (($othercontrol == CONTROLMAPDEFAULT)
                      && (!UserInGroup($groupname))));
    }

    my $group = 
    {
        'bit' => $id , 
        'checked' => $check , 
        'description' => $description 
    };

    push @groups, $group;        
}

$vars->{'group'} = \@groups;

$vars->{'default'} = \%default;

my $format = 
  GetFormat("bug/create/create", scalar $cgi->param('format'), 
            scalar $cgi->param('ctype'));

print $cgi->header($format->{'ctype'});
$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());          

