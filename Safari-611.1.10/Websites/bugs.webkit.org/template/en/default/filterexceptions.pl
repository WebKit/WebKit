# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# Important! The following classes of directives are excluded in the test,
# and so do not need to be added here. Doing so will cause warnings.
# See 008filter.t for more details.
#
# Comments                        - [%#...
# Directives                      - [% IF|ELSE|UNLESS|FOREACH...
# Assignments                     - [% foo = ...
# Simple literals                 - [% " selected" ...
# Values always used for numbers  - [% (i|j|k|n|count) %]
# Params                          - [% Param(...
# Safe functions                  - [% (time2str)...
# Safe vmethods                   - [% foo.size %] [% foo.length %]
#                                   [% foo.push() %]
# TT loop variables               - [% loop.count %]
# Already-filtered stuff          - [% wibble FILTER html %]
#   where the filter is one of html|csv|js|quoteUrls|time|uri|xml|none

%::safe = (

'whine/schedule.html.tmpl' => [
  'event.key',
  'query.id',
  'query.sort',
  'schedule.id',
  'option.0',
  'option.1',
],

'whine/mail.html.tmpl' => [
  'bug.bug_id',
],

'flag/list.html.tmpl' => [
  'flag.status', 
  'type.id', 
],

'search/form.html.tmpl' => [
  'qv.name',
  'qv.description',
],

'search/search-specific.html.tmpl' => [
  'status.name',
],

'search/tabs.html.tmpl' => [
  'content',
],

'request/queue.html.tmpl' => [
  'column_headers.$group_field', 
  'column_headers.$column', 
  'request.status', 
  'request.bug_id', 
  'request.attach_id', 
],

'reports/keywords.html.tmpl' => [
  'keyword.bug_count', 
],

'reports/report-table.csv.tmpl' => [
  'data.$tbl.$col.$row',
  'colsepchar',
],

'reports/report-table.html.tmpl' => [
  '"&amp;$col_vals" IF col_vals', 
  '"&amp;$row_vals" IF row_vals', 
  'classes.$row_idx.$col_idx', 
  'urlbase', 
  'data.$tbl.$col.$row', 
],

'reports/report.html.tmpl' => [
  'width', 
  'height', 
  'imageurl', 
  'formaturl', 
  'other_format.name', 
  'switchbase',
  'cumulate',
],

'reports/chart.html.tmpl' => [
  'width', 
  'height', 
  'imageurl', 
  'sizeurl', 
  'height + 100', 
  'height - 100', 
  'width + 100', 
  'width - 100', 
],

'reports/series-common.html.tmpl' => [
  'sel.name', 
  '"onchange=\"$sel.onchange\"" IF sel.onchange', 
],

'reports/chart.csv.tmpl' => [
  'data.$j.$i', 
  'colsepchar',
],

'reports/create-chart.html.tmpl' => [
  'series.series_id', 
  'newidx',
],

'reports/edit-series.html.tmpl' => [
  'default.series_id', 
],

'list/edit-multiple.html.tmpl' => [
  'group.id', 
  'menuname',
],

'list/list.rdf.tmpl' => [
  'template_version', 
  'bug.bug_id', 
  'column', 
],

'list/table.html.tmpl' => [
  'tableheader',
  'bug.bug_id', 
],

'list/list.csv.tmpl' => [
  'bug.bug_id', 
  'colsepchar',
],

'list/list.js.tmpl' => [
  'bug.bug_id', 
],

'global/choose-product.html.tmpl' => [
  'target',
],

# You are not permitted to add any values here. Everything in this file should 
# be filtered unless there's an extremely good reason why not, in which case,
# use the "none" dummy filter.
'global/code-error.html.tmpl' => [
],
 
'global/header.html.tmpl' => [
  'javascript', 
  'style', 
  'onload',
  'title',
  '" &ndash; $header" IF header',
  'subheader',
  'header_addl_info', 
  'message', 
],

'global/messages.html.tmpl' => [
  'series.frequency * 2',
],

'global/select-menu.html.tmpl' => [
  'options', 
  'size', 
],

'global/tabs.html.tmpl' => [
  'content', 
],

# You are not permitted to add any values here. Everything in this file should 
# be filtered unless there's an extremely good reason why not, in which case,
# use the "none" dummy filter.
'global/user-error.html.tmpl' => [
],

'global/confirm-user-match.html.tmpl' => [
  'script',
],

'bug/comments.html.tmpl' => [
  'comment.id',
  'comment.count',
  'bug.bug_id',
],

'bug/dependency-graph.html.tmpl' => [
  'image_map', # We need to continue to make sure this is safe in the CGI
  'image_url', 
  'map_url', 
  'bug_id', 
],

'bug/dependency-tree.html.tmpl' => [
  'bugid', 
  'maxdepth', 
  'hide_resolved', 
  'ids.join(",")',
  'maxdepth + 1', 
  'maxdepth > 0 && maxdepth <= realdepth ? maxdepth : ""',
  'maxdepth == 1 ? 1
                       : ( maxdepth ? maxdepth - 1 : realdepth - 1 )',
],

'bug/edit.html.tmpl' => [
  'bug.remaining_time', 
  'bug.delta_ts', 
  'bug.bug_id', 
  'group.bit', 
  'selname',
  'inputname',
  '" colspan=\"$colspan\"" IF colspan',
  '" size=\"$size\"" IF size',
  '" maxlength=\"$maxlength\"" IF maxlength',
  '" spellcheck=\"$spellcheck\"" IF spellcheck',
],

'bug/show-multiple.html.tmpl' => [
  'attachment.id', 
  'flag.status',
],

'bug/show.html.tmpl' => [
  'bug.bug_id',
],

'bug/show.xml.tmpl' => [
  'constants.BUGZILLA_VERSION', 
  'a.id', 
  'field', 
],

'bug/summarize-time.html.tmpl' => [
  'global.grand_total FILTER format("%.2f")',
  'subtotal FILTER format("%.2f")',
  'work_time FILTER format("%.2f")',
  'global.total FILTER format("%.2f")',
  'global.remaining FILTER format("%.2f")',
  'global.estimated FILTER format("%.2f")',
  'bugs.$id.remaining_time FILTER format("%.2f")',
  'bugs.$id.estimated_time FILTER format("%.2f")',
],


'bug/time.html.tmpl' => [
  "time_unit.replace('0\\Z', '')",
  '(act / (act + rem)) * 100 
       FILTER format("%d")', 
],

'bug/process/results.html.tmpl' => [
  'title.$type.ucfirst',
],

'bug/create/create.html.tmpl' => [
  'cloned_bug_id',
],

'bug/create/create-guided.html.tmpl' => [
  'sel',
],

'bug/activity/table.html.tmpl' => [
  'change.attachid', 
],

'attachment/create.html.tmpl' => [
  'bug.bug_id',
  'attachment.id', 
],

'attachment/created.html.tmpl' => [
  'attachment.id',
  'attachment.bug_id',
],

'attachment/edit.html.tmpl' => [
  'attachment.id', 
  'attachment.bug_id', 
  'editable_or_hide',
  'use_patchviewer',
],

'attachment/list.html.tmpl' => [
  'attachment.id', 
  'flag.status',
  'bugid',
  'obsolete_attachments',
],

'attachment/midair.html.tmpl' => [
  'attachment.id',
],

'attachment/show-multiple.html.tmpl' => [
  'a.id',
  'flag.status'
],

'attachment/updated.html.tmpl' => [
  'attachment.id',
],

'attachment/diff-header.html.tmpl' => [
  'attachid',
  'id',
  'bugid',
  'oldid',
  'newid',
  'patch.id',
],

'attachment/diff-file.html.tmpl' => [
  'file.minus_lines',
  'file.plus_lines',
  'section.old_start',
  'section_num',
  'current_line_old',
  'current_line_new',
],

'admin/admin.html.tmpl' => [
  'class'
],

'admin/table.html.tmpl' => [
  'contentlink'
],

'admin/custom_fields/cf-js.js.tmpl' => [
  'constants.FIELD_TYPE_SINGLE_SELECT',
  'constants.FIELD_TYPE_MULTI_SELECT',
  'constants.FIELD_TYPE_BUG_ID',
],

'admin/params/common.html.tmpl' => [
  'sortlist_separator', 
],

'admin/products/groupcontrol/confirm-edit.html.tmpl' => [
  'group.count', 
],

'admin/products/groupcontrol/edit.html.tmpl' => [
  'group.id',
  'constants.CONTROLMAPNA', 
  'constants.CONTROLMAPSHOWN',
  'constants.CONTROLMAPDEFAULT',
  'constants.CONTROLMAPMANDATORY',
],

'admin/products/list.html.tmpl' => [
  'classification_url_part', 
],

'admin/products/footer.html.tmpl' => [
  'classification_url_part', 
  'classification_text', 
],

'admin/flag-type/confirm-delete.html.tmpl' => [
  'flag_type.flag_count',
  'flag_type.id', 
],

'admin/flag-type/edit.html.tmpl' => [
  'selname',
],

'admin/flag-type/list.html.tmpl' => [
  'type.id', 
],


'admin/components/confirm-delete.html.tmpl' => [
  'comp.bug_count'
],

'admin/groups/delete.html.tmpl' => [
  'shared_queries'
],

'admin/users/confirm-delete.html.tmpl' => [
  'attachments',
  'reporter',
  'assignee_or_qa',
  'cc',
  'component_cc',
  'flags.requestee',
  'flags.setter',
  'longdescs',
  'quips',
  'series',
  'watch.watched',
  'watch.watcher',
  'whine_events',
  'whine_schedules',
  'otheruser.id'
],

'admin/users/edit.html.tmpl' => [
  'otheruser.id',
  'group.id',
],

'admin/components/edit.html.tmpl' => [
  'comp.bug_count'
],

'admin/workflow/edit.html.tmpl' => [
  'status.id',
  'new_status.id',
],

'admin/workflow/comment.html.tmpl' => [
  'status.id',
  'new_status.id',
],

'account/auth/login-small.html.tmpl' => [
  'qs_suffix',
],

'account/prefs/email.html.tmpl' => [
  'relationship.id',
  'event.id',
  'prefname',
],

'account/prefs/prefs.html.tmpl' => [
  'current_tab.label',
  'current_tab.name',
],

'account/prefs/saved-searches.html.tmpl' => [
  'group.id',
],

'config.rdf.tmpl' => [
  'escaped_urlbase',
],

);
