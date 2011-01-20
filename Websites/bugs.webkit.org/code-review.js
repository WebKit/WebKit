// Copyright (C) 2010 Adam Barth. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

(function() {
  /**
   * Create a new function with some of its arguements
   * pre-filled.
   * Taken from goog.partial in the Closure library.
   * @param {Function} fn A function to partially apply.
   * @param {...*} var_args Additional arguments that are partially
   *     applied to fn.
   * @return {!Function} A partially-applied form of the function.
   */
  function partial(fn, var_args) {
    var args = Array.prototype.slice.call(arguments, 1);
    return function() {
      // Prepend the bound arguments to the current arguments.
      var newArgs = Array.prototype.slice.call(arguments);
      newArgs.unshift.apply(newArgs, args);
      return fn.apply(this, newArgs);
    };
  };

  function determineAttachmentID() {
    try {
      return /id=(\d+)/.exec(window.location.search)[1]
    } catch (ex) {
      return;
    }
  }

  // Attempt to activate only in the "Review Patch" context.
  if (window.top != window)
    return;

  if (!window.location.search.match(/action=review/)
      && !window.location.toString().match(/bugs\.webkit\.org\/PrettyPatch/))
    return;

  var attachment_id = determineAttachmentID();
  if (!attachment_id)
    console.log('No attachment ID');

  var next_line_id = 0;
  var files = {};
  var original_file_contents = {};
  var patched_file_contents = {};
  var WEBKIT_BASE_DIR = "http://svn.webkit.org/repository/webkit/trunk/";
  var SIDE_BY_SIDE_DIFFS_KEY = 'sidebysidediffs';

  function idForLine(number) {
    return 'line' + number;
  }

  function nextLineID() {
    return idForLine(next_line_id++);
  }

  function forEachLine(callback) {
    for (var i = 0; i < next_line_id; ++i) {
      callback($('#' + idForLine(i)));
    }
  }

  function idify() {
    this.id = nextLineID();
  }

  function hoverify() {
    $(this).hover(function() {
      $(this).addClass('hot');
    },
    function () {
      $(this).removeClass('hot');
    });
  }

  function fileDiffFor(line) {
    return line.parents('.FileDiff');
  }

  function activeCommentFor(line) {
    // Scope to the diffSection as a performance improvement.
    return $('textarea[data-comment-for~="' + line[0].id + '"]', fileDiffFor(line));
  }

  function previousCommentsFor(line) {
    // Scope to the diffSection as a performance improvement.
    return $('div[data-comment-for~="' + line[0].id + '"].previousComment', fileDiffFor(line));
  }

  function findCommentPositionFor(line) {
    var previous_comments = previousCommentsFor(line);
    var num_previous_comments = previous_comments.size();
    if (num_previous_comments)
      return $(previous_comments[num_previous_comments - 1])
    return line;
  }

  function findCommentBlockFor(line) {
    var comment_block = findCommentPositionFor(line).next();
    if (!comment_block.hasClass('comment'))
      return;
    return comment_block;
  }

  function insertCommentFor(line, block) {
    findCommentPositionFor(line).after(block);
  }

  function addCommentFor(line) {
    if (line.attr('data-has-comment')) {
      // FIXME: This query is overly complex because we place comment blocks
      // after Lines.  Instead, comment blocks should be children of Lines.
      findCommentPositionFor(line).next().next().filter('.frozenComment').each(unfreezeComment);
      return;
    }
    line.attr('data-has-comment', 'true');
    line.addClass('commentContext');

    var comment_block = $('<div class="comment"><textarea data-comment-for="' + line.attr('id') + '"></textarea><div class="actions"><button class="ok">OK</button><button class="discard">Discard</button></div></div>');
    insertCommentFor(line, comment_block);
    comment_block.hide().slideDown('fast', function() {
      $(this).children('textarea').focus();
    });
  }

  function addCommentField() {
    var id = $(this).attr('data-comment-for');
    if (!id)
      id = this.id;
    addCommentFor($('#' + id));
  }

  function addPreviousComment(line, author, comment_text) {
    var line_id = line.attr('id');
    var comment_block = $('<div data-comment-for="' + line_id + '" class="previousComment"></div>');
    var author_block = $('<div class="author"></div>').text(author + ':');
    var text_block = $('<div class="content"></div>').text(comment_text);
    comment_block.append(author_block).append(text_block).each(hoverify).click(addCommentField);
    addDataCommentBaseLine(line, line_id);
    insertCommentFor(line, comment_block);
  }

  function displayPreviousComments(comments) {
    for (var i = 0; i < comments.length; ++i) {
      var author = comments[i].author;
      var file_name = comments[i].file_name;
      var line_number = comments[i].line_number;
      var comment_text = comments[i].comment_text;

      var file = files[file_name];

      var query = '.Line .to';
      if (line_number[0] == '-') {
        // The line_number represent a removal.  We need to adjust the query to
        // look at the "from" lines.
        query = '.Line .from';
        // Trim off the '-' control character.
        line_number = line_number.substr(1);
      }

      $(file).find(query).each(function() {
        if ($(this).text() != line_number)
          return;
        var line = $(this).parent();
        addPreviousComment(line, author, comment_text);
      });
    }

    var help_text = 'Scroll though diffs with the "j" and "k" keys.';
    if (comments.length == 0) {
      $('#message .commentStatus').text(help_text);
      return;
    }

    descriptor = comments.length + ' comment';
    if (comments.length > 1)
      descriptor += 's';
    $('#message .commentStatus').text('This patch has ' + descriptor + '.  Scroll through them with the "n" and "p" keys. ' + help_text);
  }

  function scanForComments(author, text) {
    var comments = []
    var lines = text.split('\n');
    for (var i = 0; i < lines.length; ++i) {
      var parts = lines[i].match(/^([> ]+)([^:]+):(-?\d+)$/);
      if (!parts)
        continue;
      var quote_markers = parts[1];
      var file_name = parts[2];
      // FIXME: Store multiple lines for multiline comments and correctly import them here.
      var line_number = parts[3];
      if (!file_name in files)
        continue;
      while (i < lines.length && lines[i].length > 0 && lines[i][0] == '>')
        ++i;
      var comment_lines = [];
      while (i < lines.length && (lines[i].length == 0 || lines[i][0] != '>')) {
        comment_lines.push(lines[i]);
        ++i;
      }
      --i; // Decrement i because the for loop will increment it again in a second.
      var comment_text = comment_lines.join('\n').trim();
      comments.push({
        'author': author,
        'file_name': file_name,
        'line_number': line_number,
        'comment_text': comment_text
      });
    }
    return comments;
  }

  function isReviewFlag(select) {
    return $(select).attr('title') == 'Request for patch review.';
  }

  function isCommitQueueFlag(select) {
    return $(select).attr('title').match(/commit-queue/);
  }

  function findControlForFlag(select) {
    if (isReviewFlag(select))
      return $('#toolbar .review select');
    else if (isCommitQueueFlag(select))
      return $('#toolbar .commitQueue select');
    return $();
  }

  function addFlagsForAttachment(details) {
    var flag_control = "<select><option></option><option>?</option><option>+</option><option>-</option></select>";
    $('#flagContainer').append(
      $('<span class="review"> r: ' + flag_control + '</span>')).append(
      $('<span class="commitQueue"> cq: ' + flag_control + '</span>'));

    details.find('#flags select').each(function() {
      var requestee = $(this).parent().siblings('td:first-child').text().trim();
      if (requestee.length) {
        // Remove trailing ':'.
        requestee = requestee.substr(0, requestee.length - 1);
        requestee = ' (' + requestee + ')';
      }
      var control = findControlForFlag(this)
      control.attr('selectedIndex', $(this).attr('selectedIndex'));
      control.parent().prepend(requestee);
    });
  }

  window.addEventListener('message', function(e) {
    if (e.origin != 'https://webkit-commit-queue.appspot.com')
      return;

    if (e.data.height) {
      $('.statusBubble')[0].style.height = e.data.height;
      $('.statusBubble')[0].style.width = e.data.width;
    }
  }, false);

  function handleStatusBubbleLoad(e) {
    e.target.contentWindow.postMessage('containerMetrics', 'https://webkit-commit-queue.appspot.com');
  }

  function fetchHistory() {
    $.get('attachment.cgi?id=' + attachment_id + '&action=edit', function(data) {
      var bug_id = /Attachment \d+ Details for Bug (\d+)/.exec(data)[1];
      $.get('show_bug.cgi?id=' + bug_id, function(data) {
        var comments = [];
        $(data).find('.bz_comment').each(function() {
          var author = $(this).find('.email').text();
          var text = $(this).find('.bz_comment_text').text();
          var comment_marker = '(From update of attachment ' + attachment_id + ' .details.)';
          if (text.match(comment_marker))
            $.merge(comments, scanForComments(author, text));
        });
        displayPreviousComments(comments);
      });

      var details = $(data);
      addFlagsForAttachment(details);

      var statusBubble = document.createElement('iframe');
      statusBubble.className = 'statusBubble';
      statusBubble.src  = 'https://webkit-commit-queue.appspot.com/status-bubble/' + attachment_id;
      statusBubble.scrolling = 'no';
      // Can't append the HTML because we need to set the onload handler before appending the iframe to the DOM.
      statusBubble.onload = handleStatusBubbleLoad;
      $('#statusBubbleContainer').append(statusBubble);

      $('#toolbar .bugLink').html('<a href="/show_bug.cgi?id=' + bug_id + '" target="_blank">Bug ' + bug_id + '</a>');
    });
  }

  function firstLine(file_diff) {
    var container = $('.LineContainer:not(.context)', file_diff)[0];
    var from = fromLineNumber(container);
    var to = toLineNumber(container);
    return from || to;
  }

  function crawlDiff() {
    $('.Line').each(idify).each(hoverify);
    $('.FileDiff').each(function() {
      var header = $(this).children('h1');
      var url_hash = '#L' + firstLine(this);

      var file_link = $('a', header)[0];
      file_link.target = "_blank";
      file_link.href += url_hash;

      var file_name = header.text();
      files[file_name] = this;

      addExpandLinks(file_name);
      addFileDiffLinks(file_name, url_hash);
    });
  }

  function addFileDiffLinks(file_name, url_hash) {
    var diff_links = $('<div class="FileDiffLinkContainer LinkContainer">' +
        diffLinksHtml() +
        '</div>');

    var trac_links = $('<a target="_blank">annotate</a><a target="_blank">revision log</a>');
    trac_links[0].href = 'http://trac.webkit.org/browser/trunk/' + file_name + '?annotate=blame' + url_hash;
    trac_links[1].href = 'http://trac.webkit.org/log/trunk/' + file_name;
    diff_links.append(trac_links);

    $('h1', files[file_name]).after(diff_links);
    updateDiffLinkVisibility(files[file_name]);
  }

  function addExpandLinks(file_name) {
    if (file_name.indexOf('ChangeLog') != -1)
      return;

    var file_diff = files[file_name];

    // Don't show the links to expand upwards/downwards if the patch starts/ends without context
    // lines, i.e. starts/ends with add/remove lines.
    var first_line = file_diff.querySelector('.LineContainer');

    // If there is no element with a "Line" class, then this is an image diff.
    if (!first_line)
      return;

    $('.context', file_diff).detach();

    var expand_bar_index = 0;
    if (!$(first_line).hasClass('add') && !$(first_line).hasClass('remove'))
      $('h1', file_diff).after(expandBarHtml(BELOW))

    $('br', file_diff).replaceWith(expandBarHtml());

    var last_line = file_diff.querySelector('.LineContainer:last-of-type');
    // Some patches for new files somehow end up with an empty context line at the end
    // with a from line number of 0. Don't show expand links in that case either.
    if (!$(last_line).hasClass('add') && !$(last_line).hasClass('remove') && fromLineNumber(last_line) != 0)
      $('.revision', file_diff).before(expandBarHtml(ABOVE));
  }

  function expandBarHtml(opt_direction) {
    var html = '<div class="ExpandBar">' +
        '<div class="ExpandArea Expand' + ABOVE + '"></div>' +
        '<div class="ExpandLinkContainer LinkContainer"><span class="ExpandText">expand: </span>';

    // FIXME: If there are <100 line to expand, don't show the expand-100 link.
    // If there are <20 lines to expand, don't show the expand-20 link.
    if (!opt_direction || opt_direction == ABOVE) {
      html += expandLinkHtml(ABOVE, 100) +
          expandLinkHtml(ABOVE, 20);
    }

    html += expandLinkHtml(ALL);

    if (!opt_direction || opt_direction == BELOW) {
      html += expandLinkHtml(BELOW, 20) +
        expandLinkHtml(BELOW, 100);
    }

    html += '</div><div class="ExpandArea Expand' + BELOW + '"></div></div>';
    return html;
  }

  function expandLinkHtml(direction, amount) {
    return "<a class='ExpandLink' href='javascript:' data-direction='" + direction + "' data-amount='" + amount + "'>" +
        (amount ? amount + " " : "") + direction + "</a>";
  }

  function handleExpandLinkClick() {
    var expand_bar = $(this).parents('.ExpandBar');
    var file_name = expand_bar.parents('.FileDiff').children('h1')[0].textContent;
    var expand_function = partial(expand, expand_bar[0], file_name, this.getAttribute('data-direction'), Number(this.getAttribute('data-amount')));
    if (file_name in original_file_contents)
      expand_function();
    else
      getWebKitSourceFile(file_name, expand_function, expand_bar);
  }

  function handleSideBySideLinkClick() {
    convertDiff('sidebyside', this);
  }

  function handleUnifyLinkClick() {
    convertDiff('unified', this);
  }

  function convertDiff(difftype, convert_link) {
    var file_diffs = $(convert_link).parents('.FileDiff');
    if (!file_diffs.size()) {
      localStorage.setItem('code-review-diffstate', difftype);
      file_diffs = $('.FileDiff');
    }

    convertAllFileDiffs(difftype, file_diffs);
  }

  function patchRevision() {
    var revision = $('.revision');
    return revision[0] ? revision.first().text() : null;
  }

  function getWebKitSourceFile(file_name, onLoad, expand_bar) {
    function handleLoad(contents) {
      original_file_contents[file_name] = contents.split('\n');
      patched_file_contents[file_name] = applyDiff(original_file_contents[file_name], file_name);
      onLoad();
    };

    var revision = patchRevision();
    var queryParameters = revision ? '?p=' + revision : '';

    $.ajax({
      url: WEBKIT_BASE_DIR + file_name + queryParameters,
      context: document.body,
      complete: function(xhr, data) {
              if (xhr.status == 0)
                  handleLoadError(expand_bar);
              else
                  handleLoad(xhr.responseText);
      }
    });
  }

  function replaceExpandLinkContainers(expand_bar, text) {
    $('.ExpandLinkContainer', $(expand_bar).parents('.FileDiff')).replaceWith('<span class="ExpandText">' + text + '</span>');
  }

  function handleLoadError(expand_bar) {
    replaceExpandLinkContainers(expand_bar, "Can't expand. Is this a new or deleted file?");
  }

  var ABOVE = 'above';
  var BELOW = 'below';
  var ALL = 'all';

  function lineNumbersFromSet(set, is_last) {
    var to = -1;
    var from = -1;

    var size = set.size();
    var start = is_last ? (size - 1) : 0;
    var end = is_last ? -1 : size;
    var offset = is_last ? -1 : 1;

    for (var i = start; i != end; i += offset) {
      if (to != -1 && from != -1)
        return {to: to, from: from};

      var line_number = set[i];
      if ($(line_number).hasClass('to')) {
        if (to == -1)
          to = Number(line_number.textContent);
      } else {
        if (from == -1)
          from = Number(line_number.textContent);
      }
    }
  }

  function expand(expand_bar, file_name, direction, amount) {
    if (file_name in original_file_contents && !patched_file_contents[file_name]) {
      // FIXME: In this case, try fetching the source file at the revision the patch was created at.
      // Might need to modify webkit-patch to include that data in the diff.
      replaceExpandLinkContainers(expand_bar, "Can't expand. Unable to apply patch to tip of tree.");
      return;
    }

    var above_expansion = expand_bar.querySelector('.Expand' + ABOVE)
    var below_expansion = expand_bar.querySelector('.Expand' + BELOW)

    var above_line_numbers = $('.expansionLineNumber', above_expansion);
    if (!above_line_numbers[0]) {
      var diff_section = expand_bar.previousElementSibling;
      above_line_numbers = $('.lineNumber', diff_section);
    }

    var above_last_line_num, above_last_from_line_num;
    if (above_line_numbers[0]) {
      var above_numbers = lineNumbersFromSet(above_line_numbers, true);
      above_last_line_num = above_numbers.to;
      above_last_from_line_num = above_numbers.from;
    } else
      above_last_from_line_num = above_last_line_num = 0;

    var below_line_numbers = $('.expansionLineNumber', below_expansion);
    if (!below_line_numbers[0]) {
      var diff_section = expand_bar.nextElementSibling;
      if (diff_section)
        below_line_numbers = $('.lineNumber', diff_section);
    }

    var below_first_line_num, below_first_from_line_num;
    if (below_line_numbers[0]) {
      var below_numbers = lineNumbersFromSet(below_line_numbers, false);
      below_first_line_num = below_numbers.to - 1;
      below_first_from_line_num = below_numbers.from - 1;
    } else
      below_first_from_line_num = below_first_line_num = patched_file_contents[file_name].length - 1;

    var start_line_num, start_from_line_num;
    var end_line_num;

    if (direction == ABOVE) {
      start_from_line_num = above_last_from_line_num;
      start_line_num = above_last_line_num;
      end_line_num = Math.min(start_line_num + amount, below_first_line_num);
    } else if (direction == BELOW) {
      end_line_num = below_first_line_num;
      start_line_num = Math.max(end_line_num - amount, above_last_line_num)
      start_from_line_num = Math.max(below_first_from_line_num - amount, above_last_from_line_num)
    } else { // direction == ALL
      start_line_num = above_last_line_num;
      start_from_line_num = above_last_from_line_num;
      end_line_num = below_first_line_num;
    }

    var lines = expansionLines(file_name, expansion_area, direction, start_line_num, end_line_num, start_from_line_num);

    var expansion_area;
    // Filling in all the remaining lines. Overwrite the expand links.
    if (start_line_num == above_last_line_num && end_line_num == below_first_line_num) {
      $('.ExpandLinkContainer', expand_bar).detach();
      below_expansion.insertBefore(lines, below_expansion.firstChild);
    } else if (direction == ABOVE) {
      above_expansion.appendChild(lines);
    } else {
      below_expansion.insertBefore(lines, below_expansion.firstChild);
    }
  }

  function unifiedLine(from, to, contents, is_expansion_line, opt_className, opt_attributes) {
    var className = is_expansion_line ? 'ExpansionLine' : 'LineContainer Line';
    if (opt_className)
      className += ' ' + opt_className;

    var lineNumberClassName = is_expansion_line ? 'expansionLineNumber' : 'lineNumber';

    var line = $('<div class="' + className + '" ' + (opt_attributes || '') + '>' +
        '<span class="from ' + lineNumberClassName + '">' + (from || '&nbsp;') +
        '</span><span class="to ' + lineNumberClassName + '">' + (to || '&nbsp;') +
        '</span> <span class="text"></span>' +
        '</div>');

    $('.text', line).replaceWith(contents);
    return line;
  }

  function unifiedExpansionLine(from, to, contents) {
    return unifiedLine(from, to, contents, true);
  }

  function sideBySideExpansionLine(from, to, contents) {
    var line = $('<div class="ExpansionLine"></div>');
    // Clone the contents so we have two copies we can put back in the DOM.
    line.append(lineSide('from', contents.clone(true), true, from));
    line.append(lineSide('to', contents, true, to));
    return line;
  }

  function lineSide(side, contents, is_expansion_line, opt_line_number, opt_attributes, opt_class) {
    var class_name = '';
    if (opt_attributes || opt_class) {
      class_name = 'class="';
      if (opt_attributes)
        class_name += is_expansion_line ? 'ExpansionLine' : 'Line';
      class_name += ' ' + (opt_class || '') + '"';
    }

    var attributes = opt_attributes || '';

    var line_side = $('<div class="LineSide">' +
        '<div ' + attributes + ' ' + class_name + '>' +
          '<span class="' + side + ' ' + (is_expansion_line ? 'expansionLineNumber' : 'lineNumber') + '">' +
              (opt_line_number || '&nbsp;') +
          '</span>' +
          '<span class="text"></span>' +
        '</div>' +
        '</div>');

    $('.text', line_side).replaceWith(contents);
    return line_side;
  }

  function expansionLines(file_name, expansion_area, direction, start_line_num, end_line_num, start_from_line_num) {
    var fragment = document.createDocumentFragment();
    var is_side_by_side = isDiffSideBySide(files[file_name]);

    for (var i = 0; i < end_line_num - start_line_num; i++) {
      var from = start_from_line_num + i + 1;
      var to = start_line_num + i + 1;
      var contents = $('<span class="text"></span>');
      contents.text(patched_file_contents[file_name][start_line_num + i]);
      var line = is_side_by_side ? sideBySideExpansionLine(from, to, contents) : unifiedExpansionLine(from, to, contents);
      fragment.appendChild(line[0]);
    }

    return fragment;
  }

  function hunkStartingLine(patched_file, context, prev_line, hunk_num) {
    var PATCH_FUZZ = 2;
    var current_line = -1;
    var last_context_line = context[context.length - 1];
    if (patched_file[prev_line] == last_context_line)
      current_line = prev_line + 1;
    else {
      for (var i = prev_line - PATCH_FUZZ; i < prev_line + PATCH_FUZZ; i++) {
        if (patched_file[i] == last_context_line)
          current_line = i + 1;
      }

      if (current_line == -1) {
        console.log('Hunk #' + hunk_num + ' FAILED.');
        return -1;
      }
    }

    // For paranoia sake, confirm the rest of the context matches;
    for (var i = 0; i < context.length - 1; i++) {
      if (patched_file[current_line - context.length + i] != context[i]) {
        console.log('Hunk #' + hunk_num + ' FAILED. Did not match preceding context.');
        return -1;
      }
    }

    return current_line;
  }

  function fromLineNumber(line) {
    var node = line.querySelector('.from');
    return node ? Number(node.textContent) : 0;
  }

  function toLineNumber(line) {
    var node = line.querySelector('.to');
    return node ? Number(node.textContent) : 0;
  }

  function textContentsFor(line) {
    // Just get the first match since a side-by-side diff has two lines with text inside them for
    // unmodified lines in the diff.
    return $('.text', line).first().text();
  }

  function lineNumberForFirstNonContextLine(patched_file, line, prev_line, context, hunk_num) {
    if (context.length) {
      var prev_line_num = fromLineNumber(prev_line) - 1;
      return hunkStartingLine(patched_file, context, prev_line_num, hunk_num);
    }

    if (toLineNumber(line) == 1 || fromLineNumber(line) == 1)
      return 0;

    console.log('Failed to apply patch. Adds or removes lines before any context lines.');
    return -1;
  }

  function applyDiff(original_file, file_name) {
    var diff_sections = files[file_name].getElementsByClassName('DiffSection');
    var patched_file = original_file.concat([]);

    // Apply diffs in reverse order to avoid needing to keep track of changing line numbers.
    for (var i = diff_sections.length - 1; i >= 0; i--) {
      var section = diff_sections[i];
      var lines = section.getElementsByClassName('Line');
      var current_line = -1;
      var context = [];
      var hunk_num = i + 1;

      for (var j = 0, lines_len = lines.length; j < lines_len; j++) {
        var line = lines[j];
        var line_contents = textContentsFor(line);
        if ($(line).hasClass('add')) {
          if (current_line == -1) {
            current_line = lineNumberForFirstNonContextLine(patched_file, line, lines[j-1], context, hunk_num);
            if (current_line == -1)
              return null;
          }

          patched_file.splice(current_line, 0, line_contents);
          current_line++;
        } else if ($(line).hasClass('remove')) {
          if (current_line == -1) {
            current_line = lineNumberForFirstNonContextLine(patched_file, line, lines[j-1], context, hunk_num);
            if (current_line == -1)
              return null;
          }

          if (patched_file[current_line] != line_contents) {
            console.log('Hunk #' + hunk_num + ' FAILED.');
            return null;
          }

          patched_file.splice(current_line, 1);
        } else if (current_line == -1) {
          context.push(line_contents);
        } else if (line_contents != patched_file[current_line]) {
          console.log('Hunk #' + hunk_num + ' FAILED. Context at end did not match');
          return null;
        } else {
          current_line++;
        }
      }
    }

    return patched_file;
  }

  function openOverallComments(e) {
    $('.overallComments textarea').addClass('open');
    $('#statusBubbleContainer').addClass('wrap');
  }

  function onBodyResize() {
    updateToolbarAnchorState();
  }

  function updateToolbarAnchorState() {
    var has_scrollbar = window.innerWidth > document.documentElement.offsetWidth;
    $('#toolbar').toggleClass('anchored', has_scrollbar);
  }

  function diffLinksHtml() {
    return '<a href="javascript:" class="unify-link">unified</a>' +
      '<a href="javascript:" class="side-by-side-link">side-by-side</a>';
  }

  $(document).ready(function() {
    crawlDiff();
    fetchHistory();
    $(document.body).prepend('<div id="message">' +
        '<div class="help">Select line numbers to add a comment.' +
          '<div class="DiffLinks LinkContainer">' + diffLinksHtml() + '</div>' +
        '</div>' +
        '<div class="commentStatus"></div>' +
        '</div>');
    $(document.body).append('<div id="toolbar">' +
        '<div class="overallComments">' +
            '<textarea placeholder="Overall comments"></textarea>' +
        '</div>' +
        '<div>' +
          '<span id="statusBubbleContainer"></span>' +
          '<span class="actions">' +
              '<span class="links"><span class="bugLink"></span></span>' +
              '<span id="flagContainer"></span>' +
              '<button id="preview_comments">Preview</button>' +
              '<button id="post_comments">Publish</button> ' +
          '</span></div>' +
        '</div>' +
        '</div>');

    $('.overallComments textarea').bind('click', openOverallComments);

    $(document.body).prepend('<div id="comment_form" class="inactive"><div class="winter"></div><div class="lightbox"><iframe id="reviewform" src="attachment.cgi?id=' + attachment_id + '&action=reviewform"></iframe></div></div>');

    // Create a dummy iframe and monitor resizes in it's contentWindow to detect when the top document's body changes size.
    // FIXME: Should we setTimeout throttle these?
    var resize_iframe = $('<iframe class="pseudo_resize_event_iframe"></iframe>');
    $(document.body).append(resize_iframe);
    $(resize_iframe[0].contentWindow).bind('resize', onBodyResize);

    updateToolbarAnchorState();
    loadDiffState();
  });

  function loadDiffState() {
    var diffstate = localStorage.getItem('code-review-diffstate');
    if (diffstate != 'sidebyside' && diffstate != 'unified')
      return;

    convertAllFileDiffs(diffstate, $('.FileDiff'));
  }

  function isDiffSideBySide(file_diff) {
    return diffState(file_diff) == 'sidebyside';
  }

  function diffState(file_diff) {
    var diff_state = $(file_diff).attr('data-diffstate');
    return diff_state || 'unified';
  }

  function unifyLine(line, from, to, contents, classNames, attributes, id) {
    var new_line = unifiedLine(from, to, contents, false, classNames, attributes);
    var old_line = $(line);
    if (!old_line.hasClass('LineContainer'))
      old_line = old_line.parents('.LineContainer');

    var comments = commentsToTransferFor($(document.getElementById(id)));
    old_line.after(comments);
    old_line.replaceWith(new_line);
  }

  function updateDiffLinkVisibility(file_diff) {
    if (diffState(file_diff) == 'unified') {
      $('.side-by-side-link', file_diff).show();
      $('.unify-link', file_diff).hide();
    } else {
      $('.side-by-side-link', file_diff).hide();
      $('.unify-link', file_diff).show();
    }
  }

  function convertAllFileDiffs(diff_type, file_diffs) {
    file_diffs.each(function() {
      convertFileDiff(diff_type, this);
    });
  }

  function convertFileDiff(diff_type, file_diff) {
    if (diffState(file_diff) == diff_type)
      return;

    $(file_diff).removeClass('sidebyside unified');
    $(file_diff).addClass(diff_type);

    $(file_diff).attr('data-diffstate', diff_type);
    updateDiffLinkVisibility(file_diff);

    $('.shared .Line', file_diff).each(function() {
      convertLine(diff_type, this);
    });

    $('.ExpansionLine', file_diff).each(function() {
      convertExpansionLine(diff_type, this);
    });
  }

  function convertLine(diff_type, line) {
    var convert_function = diff_type == 'sidebyside' ? sideBySideifyLine : unifyLine;
    var from = fromLineNumber(line);
    var to = toLineNumber(line);
    var contents = $('.text', line).first();
    var classNames = classNamesForMovingLine(line);
    var attributes = attributesForMovingLine(line);
    var id = line.id;
    convert_function(line, from, to, contents, classNames, attributes, id)
  }

  function classNamesForMovingLine(line) {
    var classParts = line.className.split(' ');
    var classBuffer = [];
    for (var i = 0; i < classParts.length; i++) {
      var part = classParts[i];
      if (part != 'LineContainer' && part != 'Line')
        classBuffer.push(part);
    }
    return classBuffer.join(' ');
  }

  function attributesForMovingLine(line) {
    var attributesBuffer = ['id=' + line.id];
    // Make sure to keep all data- attributes.
    $(line.attributes).each(function() {
      if (this.name.indexOf('data-') == 0)
        attributesBuffer.push(this.name + '=' + this.value);
    });
    return attributesBuffer.join(' ');
  }

  function sideBySideifyLine(line, from, to, contents, classNames, attributes, id) {
    var from_class = '';
    var to_class = '';
    var from_attributes = '';
    var to_attributes = '';
    // Clone the contents so we have two copies we can put back in the DOM.
    var from_contents = contents.clone(true);
    var to_contents = contents;

    var container_class = 'LineContainer';
    var container_attributes = '';

    if (from && !to) { // This is a remove line.
      from_class = classNames;
      from_attributes = attributes;
      to_contents = '';
    } else if (to && !from) { // This is an add line.
      to_class = classNames;
      to_attributes = attributes;
      from_contents = '';
    } else {
      container_attributes = attributes;
      container_class += ' Line ' + classNames;
    }

    var new_line = $('<div ' + container_attributes + ' class="' + container_class + '"></div>');
    new_line.append(lineSide('from', from_contents, false, from, from_attributes, from_class));
    new_line.append(lineSide('to', to_contents, false, to, to_attributes, to_class));

    $(line).replaceWith(new_line);

    var line = $(document.getElementById(id));
    line.after(commentsToTransferFor(line));
  }

  function convertExpansionLine(diff_type, line) {
    var convert_function = diff_type == 'sidebyside' ? sideBySideExpansionLine : unifiedExpansionLine;
    var contents = $('.text', line).first();
    var from = fromLineNumber(line);
    var to = toLineNumber(line);
    var new_line = convert_function(from, to, contents);
    $(line).replaceWith(new_line);
  }

  function commentsToTransferFor(line) {
    var fragment = document.createDocumentFragment();

    previousCommentsFor(line).each(function() {
      fragment.appendChild(this);
    });

    var active_comments = activeCommentFor(line);
    var num_active_comments = active_comments.size();
    if (num_active_comments > 0) {
      if (num_active_comments > 1)
        console.log('ERROR: There is more than one active comment for ' + line.attr('id') + '.');

      var parent = active_comments[0].parentNode;
      var frozenComment = parent.nextSibling;
      fragment.appendChild(parent);
      fragment.appendChild(frozenComment);
    }

    return fragment;
  }

  function discardComment() {
    var line_id = $(this).parentsUntil('.comment').parent().find('textarea').attr('data-comment-for');
    var line = $('#' + line_id)
    findCommentBlockFor(line).slideUp('fast', function() {
      $(this).remove();
      line.removeAttr('data-has-comment');
      trimCommentContextToBefore(line, line.attr('data-comment-base-line'));
    });
  }

  function unfreezeComment() {
    $(this).prev().show();
    $(this).remove();
  }

  function showFileDiffLinks() {
    $('.LinkContainer', this).each(function() { this.style.opacity = 1; });
  }

  function hideFileDiffLinks() {
    $('.LinkContainer', this).each(function() { this.style.opacity = 0; });
  }

  $('.FileDiff').live('mouseenter', showFileDiffLinks);
  $('.FileDiff').live('mouseleave', hideFileDiffLinks);
  $('.side-by-side-link').live('click', handleSideBySideLinkClick);
  $('.unify-link').live('click', handleUnifyLinkClick);
  $('.ExpandLink').live('click', handleExpandLinkClick);
  $('.comment .discard').live('click', discardComment);
  $('.frozenComment').live('click', unfreezeComment);

  $('.comment .ok').live('click', function() {
    var comment_textarea = $(this).parentsUntil('.comment').parent().find('textarea');
    if (comment_textarea.val().trim() == '') {
      discardComment.call(this);
      return;
    }
    var line_id = comment_textarea.attr('data-comment-for');
    var line = $('#' + line_id)
    findCommentBlockFor(line).hide().after($('<div class="frozenComment"></div>').text(comment_textarea.val()));
  });

  function focusOn(node) {
    $('.focused').removeClass('focused');
    if (node.length == 0)
      return;
    $(document).scrollTop(node.addClass('focused').position().top - window.innerHeight / 2);
  }

  function focusNext(className, is_backward) {
    var focusable_nodes = $('.previousComment,.DiffBlock').filter(function() {
      return ($(this).hasClass('previousComment') || $('.add,.remove', this).size());
    });

    var index = focusable_nodes.index($('.focused'));
    if (index == -1 && is_backward)
      index = focusable_nodes.length;

    var offset = is_backward ? -1 : 1;
    var end = is_backward ? -1 : focusable_nodes.size();
    for (var i = index + offset; i != end; i = i + offset) {
      var node = $(focusable_nodes[i]);
      if (node.hasClass(className)) {
        focusOn(node);
        return;
      }
    }
  }

  var kCharCodeForN = 'n'.charCodeAt(0);
  var kCharCodeForP = 'p'.charCodeAt(0);
  var kCharCodeForJ = 'j'.charCodeAt(0);
  var kCharCodeForK = 'k'.charCodeAt(0);

  $('body').live('keypress', function() {
    // FIXME: There's got to be a better way to avoid seeing these keypress
    // events.
    if (event.target.nodeName == 'TEXTAREA')
      return;

    switch (event.charCode) {
    case kCharCodeForN:
      focusNext('previousComment', false);
      break;

    case kCharCodeForP:
      focusNext('previousComment', true);
      break;

    case kCharCodeForJ:
      focusNext('DiffBlock', false);
      break;

    case kCharCodeForK:
      focusNext('DiffBlock', true);
      break;
    }
  });

  function contextLinesFor(comment_base_lines, file_diff) {
    var base_lines = comment_base_lines.split(' ');
    return $('div[data-comment-base-line]', file_diff).filter(function() {
      return $(this).attr('data-comment-base-line').split(' ').some(function(item) {
        return base_lines.indexOf(item) != -1;
      });
    });
  }

  function numberFrom(line_id) {
    return Number(line_id.replace('line', ''));
  }

  function trimCommentContextToBefore(line, comment_base_line) {
    var line_to_trim_to = numberFrom(line.attr('id'));
    contextLinesFor(comment_base_line, fileDiffFor(line)).each(function() {
      var id = $(this).attr('id');
      if (numberFrom(id) > line_to_trim_to)
        return;

      removeDataCommentBaseLine(this, comment_base_line);
      if (!$(this).attr('data-comment-base-line'))
        $(this).removeClass('commentContext');
    });
  }

  var drag_select_start_index = -1;

  function stopDragSelect() {
    $('.selected').removeClass('selected');
    drag_select_start_index = -1;
  }

  function lineOffsetFrom(line, offset) {
    var file_diff = line.parents('.FileDiff');
    var all_lines = $('.Line', file_diff);
    var index = all_lines.index(line);
    return $(all_lines[index + offset]);
  }

  function previousLineFor(line) {
    return lineOffsetFrom(line, -1);
  }

  function nextLineFor(line) {
    return lineOffsetFrom(line, 1);
  }

  $('.lineNumber').live('click', function() {
    var line = lineFromLineDescendant($(this));
    if (line.hasClass('commentContext'))
      trimCommentContextToBefore(previousLineFor(line), line.attr('data-comment-base-line'));
  }).live('mousedown', function() {
    var line = lineFromLineDescendant($(this));
    drag_select_start_index = numberFrom(line.attr('id'));
    line.addClass('selected');
    event.preventDefault();
  });

  function selectTo(focus_index) {
    var selected = $('.selected').removeClass('selected');
    var is_backward = drag_select_start_index > focus_index;
    var current_index = is_backward ? focus_index : drag_select_start_index;
    var last_index = is_backward ? drag_select_start_index : focus_index;
    while (current_index <= last_index) {
      $('#line' + current_index).addClass('selected')
      current_index++;
    }
  }

  function selectToLineContainer(line_container) {
    var line = lineFromLineContainer(line_container);
    selectTo(numberFrom(line.attr('id')));
  }

  $('.LineContainer').live('mouseenter', function() {
    if (drag_select_start_index == -1)
      return;
    selectToLineContainer(this);
  }).live('mouseup', function() {
    if (drag_select_start_index == -1)
      return;

    selectToLineContainer(this);

    var selected = $('.selected');
    var already_has_comment = selected.last().hasClass('commentContext');
    selected.addClass('commentContext');

    var comment_base_line;
    if (already_has_comment)
      comment_base_line = selected.last().attr('data-comment-base-line');
    else {
      var last = lineFromLineDescendant(selected.last());
      addCommentFor($(last));
      comment_base_line = last.attr('id');
    }

    selected.each(function() {
      addDataCommentBaseLine(this, comment_base_line);
    });
  });

  function addDataCommentBaseLine(line, id) {
    var val = $(line).attr('data-comment-base-line');

    var parts = val ? val.split(' ') : [];
    for (var i = 0; i < parts.length; i++) {
      if (parts[i] == id)
        return;
    }

    parts.push(id);
    $(line).attr('data-comment-base-line', parts.join(' '));
  }

  function removeDataCommentBaseLine(line, comment_base_lines) {
    var val = $(line).attr('data-comment-base-line');
    if (!val)
      return;

    var base_lines = comment_base_lines.split(' ');
    var parts = val.split(' ');
    var newVal = [];
    for (var i = 0; i < parts.length; i++) {
      if (base_lines.indexOf(parts[i]) == -1)
        newVal.push(parts[i]);
    }

    $(line).attr('data-comment-base-line', newVal.join(' '));
  }

  function lineFromLineDescendant(descendant) {
    return descendant.hasClass('Line') ? descendant : descendant.parents('.Line');
  }

  function lineFromLineContainer(lineContainer) {
    var line = $(lineContainer);
    if (!line.hasClass('Line'))
      line = $('.Line', line);
    return line;
  }

  $('.DiffSection').live('mouseleave', stopDragSelect).live('mouseup', stopDragSelect);

  function contextSnippetFor(line, indent) {
    var snippets = []
    contextLinesFor(line.attr('id'), fileDiffFor(line)).each(function() {
      var action = ' ';
      if ($(this).hasClass('add'))
        action = '+';
      else if ($(this).hasClass('remove'))
        action = '-';
      snippets.push(indent + action + textContentsFor(this));
    });
    return snippets.join('\n');
  }

  function fileNameFor(line) {
    return fileDiffFor(line).find('h1').text();
  }

  function indentFor(depth) {
    return (new Array(depth + 1)).join('>') + ' ';
  }

  function snippetFor(line, indent) {
    var file_name = fileNameFor(line);
    var line_number = line.hasClass('remove') ? '-' + fromLineNumber(line[0]) : toLineNumber(line[0]);
    return indent + file_name + ':' + line_number + '\n' + contextSnippetFor(line, indent);
  }

  function quotePreviousComments(comments) {
    var quoted_comments = [];
    var depth = comments.size();
    comments.each(function() {
      var indent = indentFor(depth--);
      var text = $(this).children('.content').text();
      quoted_comments.push(indent + '\n' + indent + text.split('\n').join('\n' + indent));
    });
    return quoted_comments.join('\n');
  }

  $('#comment_form .winter').live('click', function() {
    $('#comment_form').addClass('inactive');
  });

  function fillInReviewForm() {
    var comments_in_context = []
    forEachLine(function(line) {
      if (line.attr('data-has-comment') != 'true')
        return;
      var comment = findCommentBlockFor(line).children('textarea').val().trim();
      if (comment == '')
        return;
      var previous_comments = previousCommentsFor(line);
      var snippet = snippetFor(line, indentFor(previous_comments.size() + 1));
      var quoted_comments = quotePreviousComments(previous_comments);
      var comment_with_context = [];
      comment_with_context.push(snippet);
      if (quoted_comments != '')
        comment_with_context.push(quoted_comments);
      comment_with_context.push('\n' + comment);
      comments_in_context.push(comment_with_context.join('\n'));
    });
    var comment = $('.overallComments textarea').val().trim();
    if (comment != '')
      comment += '\n\n';
    comment += comments_in_context.join('\n\n');
    if (comments_in_context.length > 0)
      comment = 'View in context: ' + window.location + '\n\n' + comment;
    var review_form = $('#reviewform').contents();
    review_form.find('#comment').val(comment);
    review_form.find('#flags select').each(function() {
      var control = findControlForFlag(this);
      if (!control.size())
        return;
      $(this).attr('selectedIndex', control.attr('selectedIndex'));
    });
  }

  $('#preview_comments').live('click', function() {
    fillInReviewForm();
    $('#comment_form').removeClass('inactive');
  });

  $('#post_comments').live('click', function() {
    fillInReviewForm();
    $('#reviewform').contents().find('form').submit();
  });
})();
