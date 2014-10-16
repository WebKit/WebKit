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
# The Original Code are the Bugzilla tests.
#
# The Initial Developer of the Original Code is Jacob Steenhagen.
# Portions created by Jacob Steenhagen are
# Copyright (C) 2001 Jacob Steenhagen. All
# Rights Reserved.
#
# Contributor(s): Gervase Markham <gerv@gerv.net>

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
  'flag.id', 
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
  'row_total',
  'col_totals.$col',
  'grand_total', 
],

'reports/report.html.tmpl' => [
  'width', 
  'height', 
  'imageurl', 
  'formaturl', 
  'other_format.name', 
  'sizeurl', 
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

'global/per-bug-queries.html.tmpl' => [
  '" value=\"$bugids\"" IF bugids',
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

'global/site-navigation.html.tmpl' => [
  'bug.bug_id', 
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
  'time_unit FILTER format(\'%.1f\')', 
  'time_unit FILTER format(\'%.2f\')', 
  '(act / (act + rem)) * 100 
       FILTER format("%d")', 
],

'bug/process/results.html.tmpl' => [
  'title.$type', 
  '"$terms.Bug $id" FILTER bug_link(id)',
  '"$terms.bug $id" FILTER bug_link(id)',
],

'bug/create/create.html.tmpl' => [
  'cloned_bug_id',
],

'bug/create/create-guided.html.tmpl' => [
  'tablecolour',
  'sel',
  'productstring', 
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
  'a',
  'editable_or_hide',
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
  'lxr_prefix',
  'file.minus_lines',
  'file.plus_lines',
  'bonsai_prefix',
  'section.old_start',
  'section_num',
  'current_line_old',
  'current_line_new',
],

'admin/admin.html.tmpl' => [
  'class'
],

'admin/table.html.tmpl' => [
  'link_uri'
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
