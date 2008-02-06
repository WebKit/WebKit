#!/usr/bin/perl -w
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
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Gregor Fischer <fischer@suse.de>
#                 Klaas Freitag  <freitag@suse.de>
#                 Seth Landsman  <seth@dworkin.net>
#                 Ludovic Dubost <ludovic@pobox.com>
###############################################################
# Bugzilla: Create a new bug via email
###############################################################
# The email needs to be feeded to this program on STDIN. 
# This is usually done by having an entry like this in your 
# .procmailrc:
# 
#     BUGZILLA_HOME=/usr/local/httpd/htdocs/bugzilla
#     :0 c
#     |(cd $BUGZILLA_HOME/contrib; ./bug_email.pl)
# 
#
# Installation note:
#
# You need to work with bug_email.pl the MIME::Parser installed.
# 
# $Id: bug_email.pl,v 1.28 2005/07/08 02:31:43 mkanat%kerio.com Exp $
###############################################################

# 02/12/2000 (SML)
# - updates to work with most recent database changes to the bugs database
# - updated so that it works out of bugzilla/contrib
# - initial checkin into the mozilla CVS tree (yay)

# 02/13/2000 (SML)
# - email transformation code.  
#   EMAIL_TRANSFORM_NONE does exact email matches
#   EMAIL_TRANSFORM_NAME_ONLY matches on the username 
#   EMAIL_TRANSFORM_BASE_DOMAIN matches on the username and checks the domain of
#    to see that the one in the database is a subset of the one in the sender address
#    this is probably prone to false positives and probably needs more work.

# 03/07/2000 (SML)
# - added in $DEFAULT_PRODUCT and $DEFAULT_COMPONENT.  i.e., if $DEFAULT_PRODUCT = "PENDING",
#    any email submitted bug will be entered with a product of PENDING, if no other product is
#    specified in the email.

# 10/21/2003 (Ludovic)
# - added $DEFAULT_VERSION, similar to product and component above
# - added command line switches to override version, product, and component, so separate
#   email addresses can be used for different product/component/version combinations.
#   Example for procmail:
#    # Feed mail to stdin of bug_email.pl
#    :0 Ec
#    * !^Subject: .*[Bug .*]
#    RESULT=|(cd $BUGZILLA_HOME/contrib && ./bug_email.pl -p='Tier_3_Operations' -c='General' )

# Next round of revisions :
# - querying a bug over email
# - appending a bug over email
# - keywords over email
# - use the globals.pl parameters functionality to edit and save this script's parameters
# - integrate some setup in the checksetup.pl script
# - gpg signatures for security

use strict;
use MIME::Parser;

BEGIN {
    chdir '..';        # this script lives in contrib
    push @INC, "contrib/.";
    push @INC, ".";
}

require "globals.pl";
use BugzillaEmail;
use Bugzilla::Config qw(:DEFAULT $datadir);

use lib ".";
use lib "../";
use Bugzilla::Constants;
use Bugzilla::BugMail;
use Bugzilla::User;

my @mailerrors = ();       # Buffer for Errors in the mail
my @mailwarnings = ();     # Buffer for Warnings found in the mail
my $critical_err = 0; # Counter for critical errors - must be zero for success
my %Control;
my $Header = "";
my @RequiredLabels = ();
my @AllowedLabels = ();
my $Body = "";
my @attachments = ();

my $product_valid = 0;
my $test = 0;
my $restricted = 0;
my $SenderShort;
my $Message_ID;

my $dbh = Bugzilla->dbh;

# change to use default product / component functionality
my $DEFAULT_PRODUCT = "PENDING";
my $DEFAULT_COMPONENT = "PENDING";
my $DEFAULT_VERSION = "unspecified";

###############################################################
# storeAttachments
# 
# in this sub, attachments found in the dump-sub will be written to 
# the database. The info, which attachments need saving is stored
# in the global @attachments-list.
# The sub returns the number of stored attachments.
sub storeAttachments( $$ )
{
    my ($bugid, $submitter_id ) = @_;
    my $maxsize = 0;
    my $data;
    my $listref  = \@attachments;
    my $att_count = 0;
    
    $submitter_id ||= 0;

    foreach my $pairref ( @$listref ) {
        my ($decoded_file, $mime, $on_disk, $description) = @$pairref;


        # Size check - mysql has a maximum space for the data ?
        $maxsize = 1047552;  # should be queried by a system( "mysqld --help" );,
        # but this seems not to be supported by all current mysql-versions

        # Read data file binary
        if( $on_disk ) {
            if( open( FILE, "$decoded_file" )) {
                binmode FILE;
                read FILE, $data, $maxsize;
                close FILE;
                $att_count ++;
            } else { 
                print "Error while reading attachment $decoded_file!\n";
                next;
            }
            # print "unlinking $datadir/mimedump-tmp/$decoded_file";
            # unlink "$datadir/mimedump-tmp/$decoded_file";
        } else {
            # data is in the scalar 
            $data = $decoded_file;
        }


        # Make SQL-String
        my $sql = "insert into attachments (bug_id, creation_ts, description, mimetype, ispatch, filename, thedata, submitter_id) values (";
        $sql .= "$bugid, now(), " . SqlQuote( $description ) . ", ";
        $sql .= SqlQuote( $mime ) . ", ";
        $sql .= "0, ";
        $sql .= SqlQuote( $decoded_file ) . ", ";
        $sql .= SqlQuote( $data ) . ", ";
        $sql .= "$submitter_id );";
        SendSQL( $sql ) unless( $test );
    }
    
    return( $att_count );
}



###############################################################
# Beautification
sub horLine( )
{
    return( "-----------------------------------------------------------------------\n" ); 
}


###############################################################
# Check if $Name is in $GroupName

# This is no more CreateBugs group, so I'm using this routine to just determine if the user is
# in the database.  Eventually, here should be a seperate routine or renamed, or something (SML)
sub CheckPermissions {
    my ($GroupName, $Name) = @_;
    
#    SendSQL("select login_name from profiles,groups where groups.name='$GroupName' and profiles.groupset & groups.bit = groups.bit and profiles.login_name=\'$Name\'");
#    my $NewName = FetchOneColumn();
#    if ( $NewName eq $Name ) {
#       return $Name;
#    } else {
#       return;
#    }
#    my $query = "SELECT login_name FROM profiles WHERE profiles.login_name=\'$Name\'";
#    SendSQL($query);
#    my $check_name = FetchOneColumn();
#    if ($check_name eq $Name) {
#      return $Name;
#    } else {
#      return;
#    }
    return findUser($Name);
}

###############################################################
# Check if product is valid.
sub CheckProduct {
    my $Product = shift;
    
    SendSQL("select name from products where name = " . SqlQuote($Product));
    my $Result = FetchOneColumn();
    if (lc($Result) eq lc($Product)) {
        return $Result;
    } else {
        return "";
    }
}

###############################################################
# Check if component is valid for product.
sub CheckComponent {
    my $Product = shift;
    my $Component = shift;
    
    SendSQL("select components.name from components, products where components.product_id = products.id AND products.name=" . SqlQuote($Product) . " and components.name=" . SqlQuote($Component));
    my $Result = FetchOneColumn();
    if (lc($Result) eq lc($Component)) {
        return $Result;
    } else {
        return "";
    }
}

###############################################################
# Check if component is valid for product.
sub CheckVersion {
    my $Product = shift;
    my $Version = shift;
    
    SendSQL("select value from versions, products where versions.product_id = products.id AND products.name=" . SqlQuote($Product) . " and value=" . SqlQuote($Version));
    my $Result = FetchOneColumn();
    if (lc($Result) eq lc($Version)) {
        return $Result;
    } else {
        return "";
    }
}

###############################################################
# Reply to a mail.
sub Reply( $$$$ ) {
    my ($Sender, $MessageID, $Subject, $Text) = @_;
    
    
    die "Cannot find sender-email-address" unless defined( $Sender );
    
    if( $test ) {
        open( MAIL, '>>', "$datadir/bug_email_test.log" );
    }
    else {
        open( MAIL, "| /usr/sbin/sendmail -t" );
    }

    print MAIL "To: $Sender\n";
    print MAIL "From: Bugzilla Mailinterface<yourmail\@here.com>\n";
    print MAIL "Subject: $Subject\n";
    print MAIL "In-Reply-To: $MessageID\n" if ( defined( $MessageID ));
    print MAIL "\n";
    print MAIL "$Text";
    close( MAIL );

}


###############################################################
# getEnumList
# Queries the Database for the table description and figures the
# enum-settings out - usefull for checking fields for enums like
# prios 
sub getEnumList( $ )
{
    my ($fieldname) = @_;
    SendSQL( "describe bugs $fieldname" );
    my ($f, $type) = FetchSQLData();

    # delete unneeded stuff
    $type =~ s/enum\(|\)//g;
    $type =~ s/\',//g;

    my @all_prios = split( /\'/, $type );
    return( @all_prios );
}

###############################################################
# CheckPriority
# Checks, if the priority setting is one of the enums defined
# in the data base
# Uses the global var. $Control{ 'priority' }
sub CheckPriority
{
    my $prio = $Control{'priority'};
    my @all_prios = getEnumList( "priority" );

    if( $prio eq "" || (lsearch( \@all_prios, $prio ) == -1)  ) {
        # OK, Prio was not defined - create Answer
        my $Text = "You sent wrong priority-setting, valid values are:" .
            join( "\n\t", @all_prios ) . "\n\n";
        $Text .= "*  The priority is set to the default value ". 
            SqlQuote( Param('defaultpriority')) . "\n";

        BugMailError( 0, $Text );

        # set default value from param-file
        $Control{'priority'} = Param( 'defaultpriority' );
    } else {
        # Nothing to do
    }
}

###############################################################
# CheckSeverity
# checks the bug_severity
sub CheckSeverity
{
    my $sever = ($Control{'bug_severity'} ||= "" );
    my @all_sever = getEnumList( "bug_severity" );

    if( (lsearch( \@all_sever, $sever ) == -1) || $sever eq "" ) {
        # OK, Prio was not defined - create Answer
        my $Text = "You sent wrong bug_severity-setting, valid values are:" .
            join( "\n\t", @all_sever ) . "\n\n";
        $Text .= "*  The bug_severity is set to the default value ". 
            SqlQuote( "normal" ) . "\n";

        BugMailError( 0, $Text );

        # set default value from param-file
        $Control{'bug_severity'} = "normal";
    } 
}

###############################################################
# CheckArea
# checks the area-field
sub CheckArea
{
    my $area = ($Control{'area'} ||= "" );
    my @all= getEnumList( "area" );

    if( (lsearch( \@all, $area ) == -1) || $area eq "" ) {
        # OK, Area was not defined - create Answer
        my $Text = "You sent wrong area-setting, valid values are:" .
            join( "\n\t", @all ) . "\n\n";
        $Text .= "*  The area is set to the default value ". 
            SqlQuote( "BUILD" ) . "\n";

        BugMailError( 0, $Text );

        # set default value from param-file
        $Control{'area'} = "BUILD";
    } 
}

###############################################################
# CheckPlatform
# checks the given Platform and corrects it
sub CheckPlatform
{
    my $platform = ($Control{'rep_platform'} ||= "" );
    my @all = getEnumList( "rep_platform" );

    if( (lsearch( \@all, $platform ) == -1) ||  $platform eq "" ) {
        # OK, Prio was not defined - create Answer
        my $Text = "You sent wrong platform-setting, valid values are:" .
            join( "\n\t", @all ) . "\n\n";
        $Text .= "*  The rep_platform is set to the default value ". 
            SqlQuote( "All" ) . "\n";

        BugMailError( 0, $Text );

        # set default value from param-file
        $Control{'rep_platform'} = "All";
    } 
}

###############################################################
# CheckSystem
# checks the given Op-Sys and corrects it
sub CheckSystem
{
    my $sys = ($Control{'op_sys'} ||= "" );
    my @all = getEnumList( "op_sys" );

    if(  (lsearch( \@all, $sys ) == -1) || $sys eq "" ) {
        # OK, Prio was not defined - create Answer
        my $Text = "You sent wrong OS-setting, valid values are:" .
            join( "\n\t", @all ) . "\n\n";
        $Text .= "*  The op_sys is set to the default value ". 
            SqlQuote( "Linux" ) . "\n";

        BugMailError( 0, $Text );

        # set default value from param-file
        $Control{'op_sys'} = "Linux";
    } 
}


###############################################################
# Fetches all lines of a query with a single column selected and
# returns it as an array
# 
sub FetchAllSQLData( )
{
    my @res = ();

    while( MoreSQLData() ){
        push( @res, FetchOneColumn() );
    }
    return( @res );
}

###############################################################
# Error Handler for Errors in the mail
# 
# This function can be called multiple within processing one mail and
# stores the errors found in the Mail. Errors are for example empty
# required tags, missing required tags and so on.
# 
# The benefit is, that the mail users get a reply, where all mail errors
# are reported. The reply mail includes all messages what was wrong and
# the second mail the user sends can be ok, cause all his faults where
# reported.
# 
# BugMailError takes two arguments: The first one is a flag, how heavy
# the error is:
# 
# 0 - Its an error, but bugzilla can process the bug. The user should
#     handle that as a warning.
# 
# 1 - Its a real bug. Bugzilla cant store the bug. The mail has to be
#     resent.
# 
# 2 - Permission error: The user does not have the permission to send
#     a bug.
# 
# The second argument is a Text which describs the bug.
# 
# 
# #
sub BugMailError($ $ )
{
    my ( $errflag, $text ) = @_;

    # On permission error, dont sent all other Errors back -> just quit !
    if( $errflag == 2 ) {            # Permission-Error
        Reply( $SenderShort, $Message_ID, "Bugzilla Error", "Permission denied.\n\n" .
               "You do not have the permissions to create a new bug. Sorry.\n" );
        exit;
    }


    # Warnings - store for the reply mail
    if( $errflag == 0 ) {
        push( @mailwarnings, $text );
    }

    # Critical Error
    if( $errflag == 1 ) {
        $critical_err += 1;
        push( @mailerrors, $text );
    }
}

###############################################################
# getWarningText()
# 
# getWarningText() returns a reply-ready Textline of all the
# Warnings in the Mail
sub getWarningText()
{
    my $anz = @mailwarnings;

    my $ret = <<END
  
The Bugzilla Mail Interface found warnings (JFYI):

END
    ;

    # Handshake if no warnings at all
    return( "\n\n Your mail was processed without Warnings !\n" ) if( $anz == 0 );

    # build a text
    $ret .= join( "\n     ", @mailwarnings );
    return( horLine() . $ret );
}

sub getErrorText()
{
    my $anz = @mailerrors;

    my $ret = <<END

**************************  ERROR  **************************
 
Your request to the Bugzilla mail interface could not be met
due to errors in the mail. We will find it !


END
    ;
    return( "\n\n Your mail was processed without errors !\n") if( $anz == 0 );
    # build a text
    $ret .= join( "\n     ", @mailerrors );
    return( $ret );
}

###############################################################
# generateTemplate
# 
# This functiuon generates a mail-Template with the 
sub generateTemplate()
{
    my $w;
    my $ret;

    # Required Labels
    $ret =<<EOF


You may want to use this template to resend your mail. Please fill in the missing
keys.

_____ snip _______________________________________________________________________

EOF
    ;
    foreach ( @RequiredLabels ) {
        $w = "";
        $w = $Control{$_} if defined( $Control{ $_ } );
        $ret .= sprintf( "    \@%-15s:  %s\n", $_, $w );
    }

    $ret .= "\n";
    # Allowed Labels
    foreach( @AllowedLabels ) {
        next if( /reporter/    );  # Reporter is not a valid label
        next if( /assigned_to/ );  # Assigned to is just a number 
        if( defined( $Control{ $_ } ) && lsearch( \@RequiredLabels, $_ ) == -1 ) {
            $ret .=  sprintf( "    \@%-15s:  %s\n", $_,  $Control{ $_ } );
        }
    }

    if( $Body eq "" ) {
    $ret .= <<END
        
   < the bug-description follows here >

_____ snip _______________________________________________________________________

END
    ; } else {
        $ret .= "\n" . $Body;
    }
        
    return( $ret );

}
#------------------------------
#
# dump_entity ENTITY, NAME
#
# Recursive routine for parsing a mime coded mail.
# One mail may contain more than one mime blocks, which need to be
# handled. Therefore, this function is called recursively.
#
# It gets the for bugzilla important information from the mailbody and 
# stores them into the global attachment-list @attachments. The attachment-list
# is needed in storeAttachments.
#
sub dump_entity {
    my ($entity, $name) = @_;
    defined($name) or $name = "'anonymous'";
    my $IO;


    # Output the body:
    my @parts = $entity->parts;
    if (@parts) {                     # multipart...
        my $i;
        foreach $i (0 .. $#parts) {       # dump each part...
            dump_entity($parts[$i], ("$name, part ".(1+$i)));
        }
    } else {                            # single part...        

        # Get MIME type, and display accordingly...
        my $msg_part = $entity->head->get( 'Content-Disposition' );
        
        $msg_part ||= ""; 

        my ($type, $subtype) = split('/', $entity->head->mime_type);
        my $body = $entity->bodyhandle;
        my ($data, $on_disk );

        if(  $msg_part =~ /^attachment/ ) {
            # Attached File
            my $des = $entity->head->get('Content-Description');
            $des ||= $entity->head->recommended_filename;
            $des ||= "unnamed attachment";

            if( defined( $body->path )) { # Data is on disk
                $on_disk = 1;
                $data = $body->path;
                
            } else {                      # Data is in core
                $on_disk = 0;
                $data = $body->as_string;
            }
            push ( @attachments, [ $data, $entity->head->mime_type, $on_disk, $des ] );
        } else {
            # Real Message
            if ($type =~ /^(text|message)$/) {     # text: display it...
                if ($IO = $body->open("r")) {
                    $Body .=  $_ while (defined($_ = $IO->getline));
                    $IO->close;
                } else {       # d'oh!
                    print "$0: couldn't find/open '$name': $!";
                }
            } else { print "Oooops - no Body !\n"; }
        }
    }
}

###############################################################
# sub extractControls
###############################################################
# 
# This sub parses the message Body and filters the control-keys. 
# Attention: Global hash Controls affected
#
sub extractControls( $ )
{
    my ($body) = @_;
    my $backbody = "";

    my @lbody = split( /\n/, $body );
    
    # In restricted mode, all lines before the first keyword
    # are skipped.
    if( $restricted ) {
        while( $lbody[0] =~ /^\s*\@.*/ ){ shift( @lbody );} 
    }
    
    # Filtering for keys
    foreach( @lbody ) {
        if( /^\s*\@description/ ) {
            s/\s*\@description//;
            $backbody .= $_;
        } elsif( /^\s*\@(.*?)(?:\s*=\s*|\s*:\s*|\s+)(.*?)\s*$/ ) {
            $Control{lc($1)} = $2;
        } else {
            $backbody .= "$_" . "\n";
        }
    }

    # thats it.
    return( $backbody );
}

###############################################################
# Main starts here
###############################################################
# 
# Commandline switches:
# -t: test mode - no DB-Inserts
foreach( @ARGV ) {
    $restricted = 1 if ( /-r/ );
    $test = 1 if ( /-t/ );

    if ( /-p=['"]?(.+)['"]?/ )
    {
      $DEFAULT_PRODUCT = $1;
    }

    if ( /-c=['"]?(.+)["']?/ )
    {
      $DEFAULT_COMPONENT = $1;
    }

    if ( /-v=['"]?(.+)["']?/ )
    {
      $DEFAULT_VERSION = $1;
    }

}

#
# Parsing a mime-message
#
if( -t STDIN ) {
print STDERR <<END
 Bugzilla Mail Interface

 This scripts reads a mail message through stdin and parses the message,
 for to insert a bug to bugzilla.

  Options
 -t: Testmode - No insert to the DB, but logfile
 -r: restricted mode - all lines before the keys in the mail are skipped

END
    ; 
exit;
}


# Create a new MIME parser:
my $parser = new MIME::Parser;

# Create and set the output directory:
# FIXME: There should be a $BUGZILLA_HOME variable (SML)
(-d "$datadir/mimedump-tmp") or mkdir "$datadir/mimedump-tmp",0755 or die "mkdir: $!";
(-w "$datadir/mimedump-tmp") or die "can't write to directory";

$parser->output_dir("$datadir/mimedump-tmp");
    
# Read the MIME message:
my $entity = $parser->read(\*STDIN) or die "couldn't parse MIME stream";
$entity->remove_sig(10);          # Removes the signature in the last 10 lines

# Getting values from parsed mail
my $Sender = $entity->get( 'From' );
$Sender ||=  $entity->get( 'Reply-To' );
$Message_ID = $entity->get( 'Message-Id' );

die (" *** Cant find Sender-adress in sent mail ! ***\n" ) unless defined( $Sender );
chomp( $Sender );
chomp( $Message_ID );

$SenderShort = $Sender;
$SenderShort =~ s/^.*?([a-zA-Z0-9_.-]+?\@[a-zA-Z0-9_.-]+\.[a-zA-Z0-9_.-]+).*$/$1/;

$SenderShort = findUser($SenderShort);

if (!defined($SenderShort)) {
  $SenderShort = $Sender;
  $SenderShort =~ s/^.*?([a-zA-Z0-9_.-]+?\@[a-zA-Z0-9_.-]+\.[a-zA-Z0-9_.-]+).*$/$1/;
}

my $Subject = "";
$Subject = $entity->get( 'Subject' );
chomp( $Subject );

# Get all the attachments
dump_entity($entity);
# print $Body;
$Body = extractControls( $Body );  # fills the Control-Hash

if( $test ) {
    foreach (keys %Control ) {
        print "$_ => $Control{$_}\n";
    }
}

$Control{'short_desc'} ||= $Subject;
#
#  * Mailparsing finishes here *
#

######################################################################
# Now a lot of Checks of the given Labels start.
# Check Control-Labels
# not: reporter !
@AllowedLabels = ("product", "version", "rep_platform",
                  "bug_severity", "priority", "op_sys", "assigned_to",
                  "bug_status", "bug_file_loc", "short_desc", "component",
                  "status_whiteboard", "target_milestone", "groupset",
                  "qa_contact");
#my @AllowedLabels = qw{Summary priority platform assign};
foreach (keys %Control) {
    if ( lsearch( \@AllowedLabels, $_) < 0 ) {
        BugMailError( 0, "You sent a unknown label: " . $_ );
    }
}

push( @AllowedLabels, "reporter" );
$Control{'reporter'} = $SenderShort;

# Check required Labels - not all labels are required, because they could be generated
# from the given information
# Just send a warning- the error-Flag will be set later
@RequiredLabels = qw{product version component short_desc};
foreach my $Label (@RequiredLabels) {
    if ( ! defined $Control{$Label} ) {
        BugMailError( 0, "You were missing a required label: \@$Label\n" );
        next;
    }

    if( $Control{$Label} =~ /^\s*$/  ) {
        BugMailError( 0, "One of your required labels is empty: $Label" );
        next;
    }
}

if ( $Body =~ /^\s*$/s ) {
    BugMailError( 1, "You sent a completely empty body !" );
}


# umask 0;

# Check Permissions ...
if (! CheckPermissions("CreateBugs", $SenderShort ) ) {
    BugMailError( 2, "Permission denied.\n\n"  .
                  "You do not have the permissions to create a new bug. Sorry.\n" );
}

# Set QA
if (Param("useqacontact")) {
    if (defined($Control{'qa_contact'}) 
        && $Control{'qa_contact'} !~ /^\s*$/ ) {
        $Control{'qa_contact'} = DBname_to_id($Control{'qa_contact'});
    } else {
        SendSQL("select initialqacontact from components, products where components.product_id = products.id AND products.name=" .
                SqlQuote($Control{'product'}) .
                " and components.name=" . SqlQuote($Control{'component'}));
        $Control{'qa_contact'} = FetchOneColumn();
    }
}

# Set Assigned - assigned_to depends on the product, cause initialowner 
#                depends on the product !
#                => first check product !
# Product
my @all_products = ();
# set to the default product.  If the default product is empty, this has no effect
my $Product = $DEFAULT_PRODUCT;
$Product = CheckProduct( $Control{'product'} ) if( defined( $Control{ 'product'} ));

if ( $Product eq "" ) {
    my $Text = "You didnt send a value for the required key \@product !\n\n";

    $Text = "You sent the invalid product \"$Control{'product'}\"!\n\n"
        if( defined( $Control{ 'product'} ));

    $Text .= "Valid products are:\n\t";

    SendSQL("select name from products ORDER BY name");
    @all_products = FetchAllSQLData();
    $Text .= join( "\n\t", @all_products ) . "\n\n";
    $Text .= horLine();

    BugMailError( 1, $Text );
} else {
    # Fill list @all_products, which is needed in case of component-help
    @all_products = ( $Product );
    $product_valid = 1;
}
$Control{'product'} = $Product;

#
# Check the Component:
#

# set to the default component.  If the default component is empty, this has no effect
my $Component = $DEFAULT_COMPONENT;

if( defined( $Control{'component' } )) {
    $Component = CheckComponent( $Control{'product'}, $Control{'component'} );
}
    
if ( $Component eq "" ) {

    my $Text = "You did not send a value for the required key \@component!\n\n"; 

    if( defined( $Control{ 'component' } )) {
        $Text = "You sent the invalid component \"$Control{'component'}\" !\n";
    }

    #
    # Attention: If no product was sent, the user needs info for all components of all
    #            products -> big reply mail :)
    #            if a product was sent, only reply the components of the sent product
    my @val_components = ();
    foreach my $prod ( @all_products ) {
        $Text .= "\nValid components for product `$prod' are: \n\t";

        SendSQL("SELECT components.name FROM components, products WHERE components.product_id=products.id AND products.name = " . SqlQuote($prod));
        @val_components = FetchAllSQLData();

        $Text .= join( "\n\t", @val_components ) . "\n";
    }
    
    # Special: if there is a valid product, maybe it has only one component -> use it !
    # 
    my $amount_of_comps = @val_components;
    if( $product_valid  && $amount_of_comps == 1 ) {
        $Component = $val_components[0];
        
        $Text .= " * You did not send a component, but a valid product " . SqlQuote( $Product ) . ".\n";
        $Text .= " * This product only has one component ". SqlQuote(  $Component ) .".\n" .
                " * This component was set by bugzilla for submitting the bug.\n\n";
        BugMailError( 0, $Text ); # No blocker

    } else { # The component is really buggy :(
        $Text  .= horLine();
        BugMailError( 1, $Text );
    }
}
$Control{'component'} = $Component;


#
# Check assigned_to
# If a value was given in the e-mail, convert it to an ID,
# otherwise, retrieve it from the database.
if ( defined($Control{'assigned_to'}) 
     && $Control{'assigned_to'} !~ /^\s*$/ ) {
    $Control{'assigned_to'} = login_to_id($Control{'assigned_to'});
} else {
    SendSQL("select initialowner from components, products where " .
            "  components.product_id=products.id AND products.name=" .
            SqlQuote($Control{'product'}) .
            " and components.name=" . SqlQuote($Control{'component'}));
    $Control{'assigned_to'} = FetchOneColumn();
}

if ( $Control{'assigned_to'} == 0 ) {
    my $Text = "Could not resolve key \@assigned_to !\n" .
        "If you do NOT send a value for assigned_to, the bug will be assigned to\n" .
            "the qa-contact for the product and component.\n";
    $Text .= "This works only if product and component are OK. \n" 
        . horLine();

    BugMailError( 1, $Text );
}


$Control{'reporter'} = login_to_id($Control{'reporter'});
if ( ! $Control{'reporter'} ) {
    BugMailError( 1, "Could not resolve reporter !\n" );
}

### Set default values
CheckPriority( );
CheckSeverity( );
CheckPlatform( );
CheckSystem( );
# CheckArea();

### Check values ...
# Version
my $Version = "$DEFAULT_VERSION";
$Version = CheckVersion( $Control{'product'}, $Control{'version'} ) if( defined( $Control{'version'}));
if ( $Version eq "" ) {
    my $Text = "You did not send a value for the required key \@version!\n\n";

    if( defined( $Control{'version'})) {
        my $Text = "You sent the invalid version \"$Control{'version'}\"!\n";
    }

    my $anz_versions;
    my @all_versions;
    # Assemble help text
    foreach my $prod ( @all_products ) {
        $Text .= "Valid versions for product " . SqlQuote( $prod ) . " are: \n\t";

        SendSQL("select value from versions, products where versions.product_id=products.id AND products.name=" . SqlQuote( $prod ));
        @all_versions = FetchAllSQLData();
        $anz_versions = @all_versions;
        $Text .= join( "\n\t", @all_versions ) . "\n" ; 

    }

    # Check if we could use the only version
    if( $anz_versions == 1 && $product_valid ) {
        $Version = $all_versions[0];
        # Fine, there is only one version string
        $Text .= " * You did not send a version, but a valid product " . SqlQuote( $Product ) . ".\n";
        $Text .= " * This product has has only the one version ". SqlQuote(  $Version) .".\n" .
            " * This version was set by bugzilla for submitting the bug.\n\n";
        $Text .= horLine();
        BugMailError( 0, $Text ); # No blocker
    } else {
        $Text .= horLine();
        BugMailError( 1, $Text );
    }

}

$Control{'version'} = $Version;

# GroupsSet: Protections for Bug info. This paramter controls the visiblility of the 
# given bug. An Error in the given Buggroup is not a blocker, a default is taken.
#
# The GroupSet is accepted only as literals linked with whitespaces, plus-signs or kommas
#
my $GroupSet = "";
my %GroupArr = ();
$GroupSet = $Control{'groupset'} if( defined( $Control{ 'groupset' }));
#
# Fetch the default value for groupsetting
my $DefaultGroup = 'ReadInternal';
SendSQL("select id from groups where name=" . SqlQuote( $DefaultGroup ));
my $default_group = FetchOneColumn();

if( $GroupSet eq "" ) {
    # Too bad: Groupset does not contain anything -> set to default
    $GroupArr{$DefaultGroup} = $default_group if(defined( $default_group ));
    #
    # Give the user a hint
    my $Text = "You did not send a value for optional key \@groupset, which controls\n";
    $Text .= "the Permissions of the bug. It will be set to a default value 'Internal Bug'\n";
    $Text .= "if the group '$DefaultGroup' exists.  The QA may change that.\n";
    
    BugMailError( 0, $Text );
} else {
    # literal  e.g. 'ReadInternal'
    my $gserr = 0;
    my $Text = "";

    #
    # Split literal Groupsettings either on Whitespaces, +-Signs or ,
    # Then search for every Literal in the DB - col name
    foreach ( split /\s+|\s*\+\s*|\s*,\s*/, $GroupSet ) {
      SendSQL("select id, Name from groups where name=" . SqlQuote($_));
        my( $bval, $bname ) = FetchSQLData();

        if( defined( $bname ) && $_ eq $bname ) {
        $GroupArr{$bname} = $bval;
        } else {
            $Text .= "You sent the wrong GroupSet-String $_\n";
            $gserr = 1;
        }
    }
    
    #
    # Give help if wrong GroupSet-String came
    if( $gserr > 0 ) {
        # There happend errors 
        $Text .= "Here are all valid literal Groupsetting-strings:\n\t";
      SendSQL( "select g.name from groups g, user_group_map u where u.user_id=".$Control{'reporter'}.
            " and g.isbuggroup=1 and g.id = u.group_id group by g.name;" );
        $Text .= join( "\n\t", FetchAllSQLData()) . "\n";
        BugMailError( 0, $Text );
    }
} # End of checking groupsets
delete $Control{'groupset'};

# ###################################################################################
# Checking is finished
#

# Check used fields
my @used_fields;

foreach my $f (@AllowedLabels) {
    if ((exists $Control{$f}) && ($Control{$f} !~ /^\s*$/ )) {
        push (@used_fields, $f);
    }
}

#
# Creating the query for inserting the bug
# -> this should only be done, if there was no critical error before
if( $critical_err == 0 )
{
    
    my $reply = <<END
  
  +---------------------------------------------------------------------------+
           B U G Z I L L A -  M A I L -  I N T E R F A C E 
  +---------------------------------------------------------------------------+

  Your Bugzilla Mail Interface request was successfull.

END
;

    $reply .= "Your Bug-ID is ";
    my $reporter = "";

    my $query = "insert into bugs (\n" . join(",\n", @used_fields ) . 
        ", bug_status, creation_ts, delta_ts, everconfirmed) values ( ";
    
    # 'Yuck'. Then again, this whole file should be rewritten anyway...
    $query =~ s/product/product_id/;
    $query =~ s/component/component_id/;

    my $tmp_reply = "These values were stored by bugzilla:\n";
    my $val;
    foreach my $field (@used_fields) {
      if( $field eq "groupset" ) {
        $query .= $Control{$field} . ",\n";
      } elsif ( $field eq 'product' ) {
          $query .= get_product_id($Control{$field}) . ",\n";
      } elsif ( $field eq 'component' ) {
          $query .= get_component_id(get_product_id($Control{'product'}),
                                     $Control{$field}) . ",\n";
      } else {
        $query .= SqlQuote($Control{$field}) . ",\n";
      }
        
      $val = $Control{ $field };
      
      $val = DBID_to_name( $val ) if( $field =~ /reporter|assigned_to|qa_contact/ );
      
      $tmp_reply .= sprintf( "     \@%-15s = %-15s\n", $field, $val );

      if ($field eq "reporter") {
        $reporter = $val;
      }
    }
    #
    # Display GroupArr
    #
    $tmp_reply .= sprintf( "     \@%-15s = %-15s\n", 'groupset', join(',', keys %GroupArr) );
    
    $tmp_reply .= "      ... and your error-description !\n";

    my $comment = $Body;
    $comment =~ s/\r\n/\n/g;     # Get rid of windows-style line endings.
    $comment =~ s/\r/\n/g;       # Get rid of mac-style line endings.
    $comment = trim($comment);

    SendSQL("SELECT now()");
    my $bug_when = FetchOneColumn();

    my $ever_confirmed = 0;
    my $state = SqlQuote("UNCONFIRMED");

    SendSQL("SELECT votestoconfirm FROM products WHERE name = " .
            SqlQuote($Control{'product'}));
    if (!FetchOneColumn()) {
      $ever_confirmed = 1;
      $state = SqlQuote("NEW");
    }

    $query .=  $state . ", \'$bug_when\', \'$bug_when\', $ever_confirmed)\n";
#    $query .=  SqlQuote( "NEW" ) . ", now(), " . SqlQuote($comment) . " )\n";

    SendSQL("SELECT userid FROM profiles WHERE " .
            $dbh->sql_istrcmp('login_name', $dbh->quote($reporter)));
    my $userid = FetchOneColumn();

    my $id;

    if( ! $test ) {
        SendSQL($query);

        $id = Bugzilla->dbh->bz_last_key('bugs', 'bug_id');

        my $long_desc_query = "INSERT INTO longdescs SET bug_id=$id, who=$userid, bug_when=\'$bug_when\', thetext=" . SqlQuote($comment);
        SendSQL($long_desc_query);

        # Cool, the mail was successful
        # system("./processmail", $id, $SenderShort);
    } else {
        $id = 0xFFFFFFFF;  # TEST !
        print "\n-------------------------------------------------------------------------\n";
        print "$query\n";
    }

    #
    # Handle GroupArr
    #
    foreach my $grp (keys %GroupArr) {
      if( ! $test) {
        SendSQL("INSERT INTO bug_group_map SET bug_id=$id, group_id=$GroupArr{$grp}");
      } else {
        print "INSERT INTO bug_group_map SET bug_id=$id, group_id=$GroupArr{$grp}\n";
      }
    }

    #
    # handle Attachments 
    #
    my $attaches = storeAttachments( $id, $Control{'reporter'} );
    $tmp_reply .= "\n\tYou sent $attaches attachment(s). \n" if( $attaches > 0 );

    $reply .= $id . "\n\n" . $tmp_reply . "\n" . getWarningText();

    $entity->purge();  # Removes all temp files

    #
    # Send the 'you did it'-reply
    Reply( $SenderShort, $Message_ID,"Bugzilla success (ID $id)", $reply );

    Bugzilla::BugMail::Send($id) if( ! $test);
    
} else {
    # There were critical errors in the mail - the bug couldnt be inserted. !
my $errreply = <<END
  
  +---------------------------------------------------------------------------+
          B U G Z I L L A -  M A I L -  I N T E R F A C E             
  +---------------------------------------------------------------------------+

END
    ;
    
    $errreply .= getErrorText() . getWarningText() . generateTemplate();

    Reply( $SenderShort, $Message_ID, "Bugzilla Error", $errreply );

    # print getErrorText();
    # print getWarningText();
    # print generateTemplate();
}





exit;

