/* The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Bugzilla Bug Tracking System.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Frédéric Buclin <LpSolit@gmail.com>
 *                 Max Kanat-Alexander <mkanat@bugzilla.org>
 *                 Edmund Wong <ewong@pw-wspx.org>
 *                 Anthony Pipkin <a.pipkin@yahoo.com>
 */

function updateCommentPrivacy(checkbox, id) {
    var comment_elem = document.getElementById('comment_text_'+id).parentNode;
    if (checkbox.checked) {
      if (!comment_elem.className.match('bz_private')) {
        comment_elem.className = comment_elem.className.concat(' bz_private');
      }
    }
    else {
      comment_elem.className =
        comment_elem.className.replace(/(\s*|^)bz_private(\s*|$)/, '$2');
    }
}

/* The functions below expand and collapse comments  */

function toggle_comment_display(link, comment_id) {
    var comment = document.getElementById('comment_text_' + comment_id);
    var re = new RegExp(/\bcollapsed\b/);
    if (comment.className.match(re))
        expand_comment(link, comment);
    else
        collapse_comment(link, comment);
}

function toggle_all_comments(action) {
    // If for some given ID the comment doesn't exist, this doesn't mean
    // there are no more comments, but that the comment is private and
    // the user is not allowed to view it.

    var comments = YAHOO.util.Dom.getElementsByClassName('bz_comment_text');
    for (var i = 0; i < comments.length; i++) {
        var comment = comments[i];
        if (!comment)
            continue;

        var id = comments[i].id.match(/\d*$/);
        var link = document.getElementById('comment_link_' + id);
        if (action == 'collapse')
            collapse_comment(link, comment);
        else
            expand_comment(link, comment);
    }
}

function collapse_comment(link, comment) {
    link.innerHTML = "[+]";
    YAHOO.util.Dom.addClass(comment, 'collapsed');
}

function expand_comment(link, comment) {
    link.innerHTML = "[-]";
    YAHOO.util.Dom.removeClass(comment, 'collapsed');
}

function wrapReplyText(text) {
    // This is -3 to account for "\n> "
    var maxCol = BUGZILLA.constant.COMMENT_COLS - 3;
    var text_lines = text.replace(/[\s\n]+$/, '').split("\n");
    var wrapped_lines = new Array();

    for (var i = 0; i < text_lines.length; i++) {
        var paragraph = text_lines[i];
        // Don't wrap already-quoted text.
        if (paragraph.indexOf('>') == 0) {
            wrapped_lines.push('> ' + paragraph);
            continue;
        }

        var replace_lines = new Array();
        while (paragraph.length > maxCol) {
            var testLine = paragraph.substring(0, maxCol);
            var pos = testLine.search(/\s\S*$/);

            if (pos < 1) {
                // Try to find some ASCII punctuation that's reasonable
                // to break on.
                var punct = '\\-\\./,!;:';
                var punctRe = new RegExp('[' + punct + '][^' + punct + ']+$');
                pos = testLine.search(punctRe) + 1;
                // Try to find some CJK Punctuation that's reasonable
                // to break on.
                if (pos == 0)
                    pos = testLine.search(/[\u3000\u3001\u3002\u303E\u303F]/) + 1;
                // If we can't find any break point, we simply break long
                // words. This makes long, punctuation-less CJK text wrap,
                // even if it wraps incorrectly.
                if (pos == 0) pos = maxCol;
            }

            var wrapped_line = paragraph.substring(0, pos);
            replace_lines.push(wrapped_line);
            paragraph = paragraph.substring(pos);
            // Strip whitespace from the start of the line
            paragraph = paragraph.replace(/^\s+/, '');
        }
        replace_lines.push(paragraph);
        wrapped_lines.push("> " + replace_lines.join("\n> "));
    }
    return wrapped_lines.join("\n") + "\n\n";
}

/* This way, we are sure that browsers which do not support JS
   * won't display this link  */

function addCollapseLink(count, title) {
    document.write(' <a href="#" class="bz_collapse_comment"' +
                   ' id="comment_link_' + count +
                   '" onclick="toggle_comment_display(this, ' +  count +
                   '); return false;" title="' + title + '">[-]<\/a> ');
}

function goto_add_comments( anchor ){
    anchor =  (anchor || "add_comment");
    // we need this line to expand the comment box
    document.getElementById('comment').focus();
    setTimeout(function(){ 
        document.location.hash = anchor;
        // firefox doesn't seem to keep focus through the anchor change
        document.getElementById('comment').focus();
    },10);
    return false;
}
