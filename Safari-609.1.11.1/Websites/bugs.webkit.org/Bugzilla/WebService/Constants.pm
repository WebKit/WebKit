# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Constants;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);

our @EXPORT = qw(
    WS_ERROR_CODE

    STATUS_OK
    STATUS_CREATED
    STATUS_ACCEPTED
    STATUS_NO_CONTENT
    STATUS_MULTIPLE_CHOICES
    STATUS_BAD_REQUEST
    STATUS_NOT_FOUND
    STATUS_GONE
    REST_STATUS_CODE_MAP

    ERROR_UNKNOWN_FATAL
    ERROR_UNKNOWN_TRANSIENT

    XMLRPC_CONTENT_TYPE_WHITELIST
    REST_CONTENT_TYPE_WHITELIST

    WS_DISPATCH
);

# This maps the error names in global/*-error.html.tmpl to numbers.
# Generally, transient errors should have a number above 0, and
# fatal errors should have a number below 0.
#
# This hash should generally contain any error that could be thrown
# by the WebService interface. If it's extremely unlikely that the
# error could be thrown (like some CodeErrors), it doesn't have to
# be listed here.
#
# "Transient" means "If you resubmit that request with different data,
# it may work."
#
# "Fatal" means, "There's something wrong with Bugzilla, probably
# something an administrator would have to fix."
#
# NOTE: Numbers must never be recycled. If you remove a number, leave a
# comment that it was retired. Also, if an error changes its name, you'll
# have to fix it here.
use constant WS_ERROR_CODE => {
    # Generic errors (Bugzilla::Object and others) are 50-99.    
    object_not_specified        => 50,
    reassign_to_empty           => 50,
    param_required              => 50,
    params_required             => 50,
    undefined_field             => 50,
    object_does_not_exist       => 51,
    param_must_be_numeric       => 52,
    number_not_numeric          => 52,
    param_invalid               => 53,
    number_too_large            => 54,
    number_too_small            => 55,
    illegal_date                => 56,
    param_integer_required      => 57,
    param_scalar_array_required => 58,
    # Bug errors usually occupy the 100-200 range.
    improper_bug_id_field_value => 100,
    bug_id_does_not_exist       => 101,
    bug_access_denied           => 102,
    bug_access_query            => 102,
    # These all mean "invalid alias"
    alias_too_long           => 103,
    alias_in_use             => 103,
    alias_is_numeric         => 103,
    alias_has_comma_or_space => 103,
    multiple_alias_not_allowed => 103,
    # Misc. bug field errors
    illegal_field => 104,
    freetext_too_long => 104,
    # Component errors
    require_component         => 105,
    component_name_too_long   => 105,
    product_unknown_component => 105,
    # Invalid Product
    no_products         => 106,
    entry_access_denied => 106,
    product_access_denied => 106,
    product_disabled    => 106,
    # Invalid Summary
    require_summary => 107,
    # Invalid field name
    invalid_field_name => 108,
    # Not authorized to edit the bug
    product_edit_denied => 109,
    # Comment-related errors
    comment_is_private => 110,
    comment_id_invalid => 111,
    comment_too_long => 114,
    comment_invalid_isprivate => 117,
    # Comment tagging
    comment_tag_disabled => 125,
    comment_tag_invalid => 126,
    comment_tag_too_long => 127,
    comment_tag_too_short => 128,
    # See Also errors
    bug_url_invalid => 112,
    bug_url_too_long => 112,
    # Insidergroup Errors
    user_not_insider => 113,
    # Note: 114 is above in the Comment-related section.
    # Bug update errors
    illegal_change => 115,
    # Dependency errors
    dependency_loop_single => 116,
    dependency_loop_multi  => 116,
    # Note: 117 is above in the Comment-related section.
    # Dup errors
    dupe_loop_detected => 118,
    dupe_id_required => 119,
    # Bug-related group errors
    group_invalid_removal => 120,
    group_restriction_not_allowed => 120,
    # Status/Resolution errors
    missing_resolution => 121,
    resolution_not_allowed => 122,
    illegal_bug_status_transition => 123,
    # Flag errors
    flag_status_invalid => 129,
    flag_update_denied => 130,
    flag_type_requestee_disabled => 131,
    flag_not_unique => 132,
    flag_type_not_unique => 133,
    flag_type_inactive => 134,

    # Authentication errors are usually 300-400.
    invalid_login_or_password => 300,
    account_disabled             => 301,
    auth_invalid_email           => 302,
    extern_id_conflict           => -303,
    auth_failure                 => 304,
    password_too_short           => 305,
    password_not_complex         => 305,
    api_key_not_valid            => 306,
    api_key_revoked              => 306,
    auth_invalid_token           => 307,

    # Except, historically, AUTH_NODATA, which is 410.
    login_required               => 410,

    # User errors are 500-600.
    account_exists        => 500,
    illegal_email_address => 501,
    auth_cant_create_account    => 501,
    account_creation_disabled   => 501,
    account_creation_restricted => 501,
    password_too_short    => 502,
    # Error 503 password_too_long no longer exists.
    invalid_username      => 504,
    # This is from strict_isolation, but it also basically means 
    # "invalid user."
    invalid_user_group    => 504,
    user_access_by_id_denied    => 505,
    user_access_by_match_denied => 505,

    # Attachment errors are 600-700.
    file_too_large         => 600,
    invalid_content_type   => 601,
    # Error 602 attachment_illegal_url no longer exists.
    file_not_specified     => 603,
    missing_attachment_description => 604,
    # Error 605 attachment_url_disabled no longer exists.
    zero_length_file       => 606,

    # Product erros are 700-800
    product_blank_name => 700,
    product_name_too_long => 701,
    product_name_already_in_use => 702,
    product_name_diff_in_case => 702,
    product_must_have_description => 703,
    product_must_have_version => 704,
    product_must_define_defaultmilestone => 705,

    # Group errors are 800-900
    empty_group_name => 800,
    group_exists => 801,
    empty_group_description => 802,
    invalid_regexp => 803,
    invalid_group_name => 804,
    group_cannot_view => 805,

    # Classification errors are 900-1000
    auth_classification_not_enabled => 900,

    # Search errors are 1000-1100
    buglist_parameters_required => 1000,

    # Flag type errors are 1100-1200
    flag_type_name_invalid        => 1101,
    flag_type_description_invalid => 1102,
    flag_type_cc_list_invalid     => 1103,
    flag_type_sortkey_invalid     => 1104,
    flag_type_not_editable        => 1105,

    # Component errors are 1200-1300
    component_already_exists => 1200,
    component_is_last        => 1201,
    component_has_bugs       => 1202,

    # Errors thrown by the WebService itself. The ones that are negative 
    # conform to http://xmlrpc-epi.sourceforge.net/specs/rfc.fault_codes.php
    xmlrpc_invalid_value => -32600,
    unknown_method       => -32601,
    json_rpc_post_only   => 32610,
    json_rpc_invalid_callback => 32611,
    xmlrpc_illegal_content_type   => 32612,
    json_rpc_illegal_content_type => 32613,
    rest_invalid_resource         => 32614,
};

# RESTful webservices use the http status code
# to describe whether a call was successful or
# to describe the type of error that occurred.
use constant STATUS_OK               => 200;
use constant STATUS_CREATED          => 201;
use constant STATUS_ACCEPTED         => 202;
use constant STATUS_NO_CONTENT       => 204;
use constant STATUS_MULTIPLE_CHOICES => 300;
use constant STATUS_BAD_REQUEST      => 400;
use constant STATUS_NOT_AUTHORIZED   => 401;
use constant STATUS_NOT_FOUND        => 404;
use constant STATUS_GONE             => 410;

# The integer value is the error code above returned by
# the related webvservice call. We choose the appropriate
# http status code based on the error code or use the
# default STATUS_BAD_REQUEST.
sub REST_STATUS_CODE_MAP {
    my $status_code_map = {
        51       => STATUS_NOT_FOUND,
        101      => STATUS_NOT_FOUND,
        102      => STATUS_NOT_AUTHORIZED,
        106      => STATUS_NOT_AUTHORIZED,
        109      => STATUS_NOT_AUTHORIZED,
        110      => STATUS_NOT_AUTHORIZED,
        113      => STATUS_NOT_AUTHORIZED,
        115      => STATUS_NOT_AUTHORIZED,
        120      => STATUS_NOT_AUTHORIZED,
        300      => STATUS_NOT_AUTHORIZED,
        301      => STATUS_NOT_AUTHORIZED,
        302      => STATUS_NOT_AUTHORIZED,
        303      => STATUS_NOT_AUTHORIZED,
        304      => STATUS_NOT_AUTHORIZED,
        410      => STATUS_NOT_AUTHORIZED,
        504      => STATUS_NOT_AUTHORIZED,
        505      => STATUS_NOT_AUTHORIZED,
        32614    => STATUS_NOT_FOUND,
        _default => STATUS_BAD_REQUEST
    };

    Bugzilla::Hook::process('webservice_status_code_map',
        { status_code_map => $status_code_map });

    return $status_code_map;
};

# These are the fallback defaults for errors not in ERROR_CODE.
use constant ERROR_UNKNOWN_FATAL     => -32000;
use constant ERROR_UNKNOWN_TRANSIENT => 32000;

use constant ERROR_GENERAL       => 999;

use constant XMLRPC_CONTENT_TYPE_WHITELIST => qw(
    text/xml
    application/xml
);

# The first content type specified is used as the default.
use constant REST_CONTENT_TYPE_WHITELIST => qw(
    application/json
    application/javascript
    text/javascript
    text/html
);

sub WS_DISPATCH {
    # We "require" here instead of "use" above to avoid a dependency loop.
    require Bugzilla::Hook;
    my %hook_dispatch;
    Bugzilla::Hook::process('webservice', { dispatch => \%hook_dispatch });

    my $dispatch = {
        'Bugzilla'         => 'Bugzilla::WebService::Bugzilla',
        'Bug'              => 'Bugzilla::WebService::Bug',
        'Classification'   => 'Bugzilla::WebService::Classification',
        'Component'        => 'Bugzilla::WebService::Component',
        'FlagType'         => 'Bugzilla::WebService::FlagType',
        'Group'            => 'Bugzilla::WebService::Group',
        'Product'          => 'Bugzilla::WebService::Product',
        'User'             => 'Bugzilla::WebService::User',
        'BugUserLastVisit' => 'Bugzilla::WebService::BugUserLastVisit',
        %hook_dispatch
    };
    return $dispatch;
};

1;

=head1 B<Methods in need of POD>

=over

=item REST_STATUS_CODE_MAP

=item WS_DISPATCH

=back
