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
# Safe functions                  - [% (time2str|GetBugLink)...
# Safe vmethods                   - [% foo.size %] [% foo.length %]
#                                   [% foo.push() %]
# TT loop variables               - [% loop.count %]
# Already-filtered stuff          - [% wibble FILTER html %]
#   where the filter is one of html|csv|js|url_quote|quoteUrls|time|uri|xml|none

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

'sidebar.xul.tmpl' => [
  'template_version', 
],

'flag/list.html.tmpl' => [
  'flag.id', 
  'flag.status', 
  'type.id', 
],

'search/boolean-charts.html.tmpl' => [
  '"field${chartnum}-${rownum}-${colnum}"', 
  '"value${chartnum}-${rownum}-${colnum}"', 
  '"type${chartnum}-${rownum}-${colnum}"', 
  'field.name', 
  'field.description', 
  'type.name', 
  'type.description', 
  '"${chartnum}-${rownum}-${newor}"', 
  '"${chartnum}-${newand}-0"', 
  'newchart',
  'jsmagic',
],

'search/form.html.tmpl' => [
  'qv.value',
  'qv.name',
  'qv.description',
  'field.name',
  'field.description',
  'field.accesskey',
  'sel.name',
  'sel.accesskey',
],

'search/search-specific.html.tmpl' => [
  'status.name',
],

'search/tabs.html.tmpl' => [
  'tab.name',
  'tab.description',
],

'request/queue.html.tmpl' => [
  'column_headers.$group_field', 
  'column_headers.$column', 
  'request.status', 
  'request.bug_id', 
  'request.attach_id', 
],

'reports/components.html.tmpl' => [
  'numcols',
  'comp.description', 
],

'reports/duplicates-table.html.tmpl' => [
  '"&amp;maxrows=$maxrows" IF maxrows',
  '"&amp;changedsince=$changedsince" IF changedsince',
  '"&amp;bug_id=$bug_ids_string&amp;sortvisible=1" 
                                                            IF sortvisible',
  'column.name', 
  'column.description',
  'bug.id', 
  'bug.count', 
  'bug.delta', 
],

'reports/duplicates.html.tmpl' => [
  'bug_ids_string', 
  'maxrows',
  'changedsince',
  'reverse',
],

'reports/keywords.html.tmpl' => [
  'keyword.description', 
  'keyword.bugcount', 
],

'reports/report-table.csv.tmpl' => [
  'num_bugs',
  'data.$tbl.$col.$row',
  'colsepchar',
],

'reports/report-table.html.tmpl' => [
  'buglistbase', 
  '"&amp;$tbl_vals" IF tbl_vals', 
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
  'imagebase', 
  'width', 
  'height', 
  'imageurl', 
  'formaturl', 
  'other_format.name', 
  'sizeurl', 
  'switchbase',
  'format',
  'cumulate',
],

'reports/duplicates.rdf.tmpl' => [
  'template_version', 
  'bug.id', 
  'bug.count', 
  'bug.delta', 
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
  'sel.accesskey', 
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

'list/change-columns.html.tmpl' => [
  'column', 
],

'list/edit-multiple.html.tmpl' => [
  'group.id', 
  'group.description',
  'group.description FILTER inactive', 
  'knum', 
  'menuname', 
],

'list/list-simple.html.tmpl' => [
  'title', 
],

'list/list.rdf.tmpl' => [
  'template_version', 
  'bug.bug_id', 
  'column', 
],

'list/table.html.tmpl' => [
  'tableheader',
  'bug.bug_id', 
  'abbrev.$id.title || field_descs.$id || column.title',
],

'list/list.csv.tmpl' => [
  'bug.bug_id', 
  'colsepchar',
],

'list/list.js.tmpl' => [
  'bug.bug_id', 
],

'global/help.html.tmpl' => [
  'h.id', 
  'h.html', 
],

'global/banner.html.tmpl' => [
  'VERSION', 
],

'global/choose-classification.html.tmpl' => [
  'classdesc.$p', 
],

'global/choose-product.html.tmpl' => [
  'target',
  'proddesc.$p', 
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
  'h1',
  'h2',
  'h3', 
  'message', 
],

'global/messages.html.tmpl' => [
  'message_tag', 
  'series.frequency * 2',
],

'global/select-menu.html.tmpl' => [
  'options', 
  'size', 
],

'global/useful-links.html.tmpl' => [
  'email', 
],

# You are not permitted to add any values here. Everything in this file should 
# be filtered unless there's an extremely good reason why not, in which case,
# use the "none" dummy filter.
'global/user-error.html.tmpl' => [
],

'global/confirm-user-match.html.tmpl' => [
  'script',
  'fields.${field_name}.flag_type.name',
],

'global/site-navigation.html.tmpl' => [
  'bug_list.first', 
  'bug_list.$prev_bug', 
  'bug_list.$next_bug', 
  'bug_list.last', 
  'bug.bug_id', 
  'bug.votes', 
],

'bug/comments.html.tmpl' => [
  'comment.isprivate', 
  'comment.when', 
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
  'dependson_ids.join(",")', 
  'blocked_ids.join(",")', 
  'dep_id', 
  'hide_resolved', 
  'maxdepth + 1', 
  'maxdepth > 0 && maxdepth <= realdepth ? maxdepth : ""',
  'maxdepth == 1 ? 1
                       : ( maxdepth ? maxdepth - 1 : realdepth - 1 )',
],

'bug/edit.html.tmpl' => [
  'bug.deadline',
  'bug.remaining_time', 
  'bug.delta_ts', 
  'bug.bug_id', 
  'bug.votes', 
  'group.bit', 
  'group.description', 
  'dep.title', 
  'dep.fieldname', 
  'accesskey', 
  'bug.${dep.fieldname}.join(\', \')', 
  'selname',
  'depbug FILTER bug_link(depbug)',
  '"${bug.dup_id}" FILTER bug_link(bug.dup_id)',
],

'bug/knob.html.tmpl' => [
  'bug.bug_id', 
  'knum', 
],

'bug/navigate.html.tmpl' => [
  'bug_list.first', 
  'bug_list.last', 
  'bug_list.$prev_bug', 
  'bug_list.$next_bug', 
],

'bug/show-multiple.html.tmpl' => [
  'bug.bug_id', 
  'bug.deadline',
],

'bug/show.xml.tmpl' => [
  'VERSION', 
  'a.attachid', 
  'field', 
],

'bug/summarize-time.html.tmpl' => [
  'global.grand_total FILTER format("%.2f")',
  'subtotal FILTER format("%.2f")',
  'work_time FILTER format("%.2f")',
  'global.total FILTER format("%.2f")',
],


'bug/time.html.tmpl' => [
  'time_unit FILTER format(\'%.1f\')', 
  'time_unit FILTER format(\'%.2f\')', 
  '(act / (act + rem)) * 100 
       FILTER format("%d")', 
],

'bug/votes/list-for-bug.html.tmpl' => [
  'voter.count', 
  'total', 
],

'bug/votes/list-for-user.html.tmpl' => [
  'product.maxperbug', 
  'bug.id', 
  'bug.count', 
  'product.total', 
  'product.maxvotes', 
],

'bug/process/confirm-duplicate.html.tmpl' => [
  'original_bug_id', 
  'duplicate_bug_id', 
],

'bug/process/midair.html.tmpl' => [
  'bug_id', 
],

'bug/process/next.html.tmpl' => [
  'bug.bug_id', 
],

'bug/process/results.html.tmpl' => [
  'title.$type', 
  'id', 
  'linktext.$type',
],

'bug/create/create.html.tmpl' => [
  'g.bit',
  'g.description',
  'sel.name',
  'sel.description',
  'cloned_bug_id'
],

'bug/create/create-guided.html.tmpl' => [
  'matches.0', 
  'tablecolour',
  'buildid',
  'sel',
  'productstring', 
],

'bug/activity/show.html.tmpl' => [
  'bug_id', 
],

'bug/activity/table.html.tmpl' => [
  'change.attachid', 
  'change.field', 
],

'attachment/create.html.tmpl' => [
  'bugid', 
  'attachment.id', 
],

'attachment/created.html.tmpl' => [
  'attachid', 
  'bugid', 
  'contenttype', 
],

'attachment/edit.html.tmpl' => [
  'attachid', 
  'bugid', 
  'a', 
],

'attachment/list.html.tmpl' => [
  'attachment.attachid', 
  'flag.status',
  'bugid',
],

'attachment/show-multiple.html.tmpl' => [
  'a.attachid', 
  'flag.status'
],

'attachment/updated.html.tmpl' => [
  'attachid', 
  'bugid', 
],

'attachment/diff-header.html.tmpl' => [
  'attachid',
  'id',
  'bugid',
  'oldid',
  'newid',
  'style',
  'javascript',
  'patch.id',
],

'attachment/diff-file.html.tmpl' => [
  'lxr_prefix',
  'file.minus_lines',
  'file.plus_lines',
  'bonsai_prefix',
  'section.old_start',
  'section_num'
],

'admin/table.html.tmpl' => [
  'link_uri',
  'c.content'
],

'admin/classifications/del.html.tmpl' => [
  'description', 
],

'admin/classifications/edit.html.tmpl' => [
  'description', 
],

'admin/classifications/reclassify.html.tmpl' => [
  'description', 
],

'admin/classifications/select.html.tmpl' => [
  'cl.description', 
],

'admin/products/groupcontrol/confirm-edit.html.tmpl' => [
  'group.count', 
],

'admin/products/groupcontrol/edit.html.tmpl' => [
  'filt_classification', 
  'filt_product', 
  'group.bugcount', 
  'group.id', 
  'const.CONTROLMAPNA', 
  'const.CONTROLMAPSHOWN', 
  'const.CONTROLMAPDEFAULT', 
  'const.CONTROLMAPMANDATORY', 
],

'admin/products/list.html.tmpl' => [
  'classification_url_part', 
],

'admin/products/confirm-delete.html.tmpl' => [
  'classification_url_part', 
  'bug_count', 
],

'admin/products/footer.html.tmpl' => [
  'classification_url_part', 
  'classification_text', 
],

'admin/keywords/edit.html.tmpl' => [
  'keyword_id',
  'bug_count',
],

'admin/keywords/confirm-delete.html.tmpl' => [
  'keyword_id',
  'bug_count',
],

'admin/flag-type/confirm-delete.html.tmpl' => [
  'flag_count', 
  'flag_type.id', 
],

'admin/flag-type/edit.html.tmpl' => [
  'action', 
  'type.id', 
  'type.target_type', 
  'type.sortkey || 1',
  'typeLabelLowerPlural',
  'typeLabelLowerSingular',
],

'admin/flag-type/list.html.tmpl' => [
  'type.id', 
  'type.flag_count', 
],


'admin/components/confirm-delete.html.tmpl' => [
  'bug_count'
],

'admin/components/deleted.html.tmpl' => [
  'deleted_bug_count'
],

'admin/users/confirm-delete.html.tmpl' => [
  'andstring',
  'responsibilityterms.$responsibility',
  'reporter',
  'assignee_or_qa',
  'cc',
  'flags.requestee',
  'flags.setter',
  'longdescs',
  'namedqueries',
  'votes',
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
  'perms.directbless',
  'perms.directmember',
],

'admin/components/edit.html.tmpl' => [
  'bug_count'
],

'admin/components/list.html.tmpl' => [
  'cgi.query_string'
],

'admin/components/select-product.html.tmpl' => [
  'cgi.query_string'
],

'admin/milestones/confirm-delete.html.tmpl' => [
  'bug_count'
],

'admin/milestones/deleted.html.tmpl' => [
  'bug_count'
],

'admin/versions/confirm-delete.html.tmpl' => [
  'bug_count'
],

'admin/settings/edit.html.tmpl' => [
  'name',
  'checkbox_name'
],

'account/login.html.tmpl' => [
  'target', 
],

'account/prefs/email.html.tmpl' => [
  'relationship.id',
  'event.id',
  'prefname',
],

'account/prefs/permissions.html.tmpl' => [
  'bit_description.name', 
  'bit_description.desc', 
],

'account/prefs/prefs.html.tmpl' => [
  'tab.name', 
  'tab.description', 
  'current_tab.name', 
  'current_tab.description', 
],

'account/prefs/settings.html.tmpl' => [
  'name',
  'default_name'
],

);
