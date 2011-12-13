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
var CODE_REVIEW_UNITTEST;

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

  if (!CODE_REVIEW_UNITTEST && !window.location.search.match(/action=review/)
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
  var g_displayed_draft_comments = false;
  var KEY_CODE = {
    down: 40,
    enter: 13,
    escape: 27,
    j: 74,
    k: 75,
    n: 78,
    p: 80,
    r: 82,
    up: 38
  }

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
    return $(line).parents('.FileDiff');
  }

  function diffSectionFor(line) {
    return $(line).parents('.DiffSection');
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

  function addDraftComment(start_line_id, end_line_id, contents) {
    var line = $('#' + end_line_id);
    var start = numberFrom(start_line_id);
    var end = numberFrom(end_line_id);
    for (var i = start; i <= end; i++) {
      addDataCommentBaseLine($('#line' + i), end_line_id);
    }

    var comment_block = createCommentFor(line);
    $(comment_block).children('textarea').val(contents);
    freezeComment(comment_block);
  }

  function ensureDraftCommentsDisplayed() {
    if (g_displayed_draft_comments)
      return;
    g_displayed_draft_comments = true;

    var comments = g_draftCommentSaver.saved_comments();
    var errors = [];
    $(comments.comments).each(function() {
      try {
        addDraftComment(this.start_line_id, this.end_line_id, this.contents);
      } catch (e) {
        errors.push({'start': this.start_line_id, 'end': this.end_line_id, 'contents': this.contents});
      }
    });
    
    if (errors.length) {
      console.log('DRAFT COMMENTS WITH ERRORS:', JSON.stringify(errors));
      alert('Some draft comments failed to be added. See the console to manually resolve.');
    }
    
    var overall_comments = comments['overall-comments'];
    if (overall_comments) {
      openOverallComments();
      $('.overallComments textarea').val(overall_comments);
    }
  }

  function DraftCommentSaver(opt_attachment_id, opt_localStorage) {
    this._attachment_id = opt_attachment_id || attachment_id;
    this._localStorage = opt_localStorage || localStorage;
    this._save_comments = true;
  }

  DraftCommentSaver.prototype._json = function() {
    var comments = $('.comment');
    var comment_store = [];
    comments.each(function () {
      var file_diff = fileDiffFor(this);
      var textarea = $('textarea', this);

      var contents = textarea.val().trim();
      if (!contents)
        return;

      var comment_base_line = textarea.attr('data-comment-for');
      var lines = contextLinesFor(comment_base_line, file_diff);

      comment_store.push({
        start_line_id: lines.first().attr('id'),
        end_line_id: comment_base_line,
        contents: contents
      });
    });

    var overall_comments = $('.overallComments textarea').val().trim();
    return JSON.stringify({'born-on': Date.now(), 'comments': comment_store, 'overall-comments': overall_comments});
  }
  
  DraftCommentSaver.prototype.localStorageKey = function() {
    return DraftCommentSaver._keyPrefix + this._attachment_id;
  }
  
  DraftCommentSaver.prototype.saved_comments = function() {
    var serialized_comments = this._localStorage.getItem(this.localStorageKey());
    if (!serialized_comments)
      return [];

    var comments = {};
    try {
      comments = JSON.parse(serialized_comments);
    } catch (e) {
      this._erase_corrupt_comments();
      return {};
    }

    var individual_comments = comments.comments;
    if (!comments || !comments['born-on'] || !individual_comments || (individual_comments.length && !individual_comments[0].contents)) {
      this._erase_corrupt_comments();
      return {};
    }    
    return comments;
  }
  
  DraftCommentSaver.prototype._erase_corrupt_comments = function() {
    // FIXME: Show an error to the user instead of logging.
    console.log('Draft comments were corrupted. Erasing comments.');
    this.erase();
  }
  
  DraftCommentSaver.prototype.save = function() {
    if (!this._save_comments)
      return;

    var key = this.localStorageKey();
    var value = this._json();

    if (this._attemptToWrite(key, value))
      return;

    this._eraseOldCommentsForAllReviews();
    if (this._attemptToWrite(key, value))
      return;

    var remove_comments = this._should_remove_comments();
    if (!remove_comments) {
      this._save_comments = false;
      return;
    }

    this._eraseCommentsForAllReviews();
    if (this._attemptToWrite(key, value))
      return;

    this._save_comments = false;
    // FIXME: Show an error to the user.
  }

  DraftCommentSaver.prototype._should_remove_comments = function(message) {
    return prompt('Local storage quota is full. Remove draft comments from all previous reviews to make room?');
  }

  DraftCommentSaver.prototype._attemptToWrite = function(key, value) {
    try {
      this._localStorage.setItem(key, value);
      return true;
    } catch (e) {
      return false;
    }
  }

  DraftCommentSaver._keyPrefix = 'draft-comments-for-attachment-';

  DraftCommentSaver.prototype.erase = function() {
    this._localStorage.removeItem(this.localStorageKey());
  }

  DraftCommentSaver.prototype._eraseOldCommentsForAllReviews = function() {
    this._eraseComments(true);
  }
  DraftCommentSaver.prototype._eraseCommentsForAllReviews = function() {
    this._eraseComments(false);
  }

  var MONTH_IN_MS = 1000 * 60 * 60 * 24 * 30;

  DraftCommentSaver.prototype._eraseComments = function(only_old_reviews) {
    var length = this._localStorage.length;
    var keys_to_delete = [];
    for (var i = 0; i < length; i++) {
      var key = this._localStorage.key(i);
      if (key.indexOf(DraftCommentSaver._keyPrefix) != 0)
        continue;
        
      if (only_old_reviews) {
        try {
          var born_on = JSON.parse(this._localStorage.getItem(key))['born-on'];
          if (Date.now() - born_on < MONTH_IN_MS)
            continue;
        } catch (e) {
          console.log('Deleting JSON. JSON for code review is corrupt: ' + key);
        }        
      }
      keys_to_delete.push(key);
    }

    for (var i = 0; i < keys_to_delete.length; i++) {
      this._localStorage.removeItem(keys_to_delete[i]);
    }
  }
  
  var g_draftCommentSaver = new DraftCommentSaver();

  function saveDraftComments() {
    ensureDraftCommentsDisplayed();
    g_draftCommentSaver.save();
    setAutoSaveStateIndicator('saved');
  }

  function setAutoSaveStateIndicator(state) {
    var container = $('.autosave-state');
    container.text(state);
    
    if (state == 'saving')
      container.addClass(state);
    else
      container.removeClass('saving');
  }
  
  function unfreezeCommentFor(line) {
    // FIXME: This query is overly complex because we place comment blocks
    // after Lines.  Instead, comment blocks should be children of Lines.
    findCommentPositionFor(line).next().next().filter('.frozenComment').each(handleUnfreezeComment);
  }

  function createCommentFor(line) {
    if (line.attr('data-has-comment')) {
      unfreezeCommentFor(line);
      return;
    }
    line.attr('data-has-comment', 'true');
    line.addClass('commentContext');

    var comment_block = $('<div class="comment"><textarea data-comment-for="' + line.attr('id') + '"></textarea><div class="actions"><button class="ok">OK</button><button class="discard">Discard</button></div></div>');
    $('textarea', comment_block).bind('input', handleOverallCommentsInput);
    insertCommentFor(line, comment_block);
    return comment_block;
  }

  function addCommentFor(line) {
    var comment_block = createCommentFor(line);
    if (!comment_block)
      return;

    comment_block.hide().slideDown('fast', function() {
      $(this).children('textarea').focus();
    });
    return comment_block;
  }

  function addCommentField(comment_block) {
    var id = $(comment_block).attr('data-comment-for');
    if (!id)
      id = comment_block.id;
    return addCommentFor($('#' + id));
  }
  
  function handleAddCommentField() {
    addCommentField(this);
  }

  function addPreviousComment(line, author, comment_text) {
    var line_id = $(line).attr('id');
    var comment_block = $('<div data-comment-for="' + line_id + '" class="previousComment"></div>');
    var author_block = $('<div class="author"></div>').text(author + ':');
    var text_block = $('<div class="content"></div>').text(comment_text);
    comment_block.append(author_block).append(text_block).each(hoverify).click(handleAddCommentField);
    addDataCommentBaseLine($(line), line_id);
    insertCommentFor($(line), comment_block);
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

    if (comments.length == 0) {
      return;
    }

    descriptor = comments.length + ' comment';
    if (comments.length > 1)
      descriptor += 's';
    $('.help .more').before(' This patch has ' + descriptor + '.  Scroll through them with the "n" and "p" keys. ');
  }
  
  function showMoreHelp() {
    $('.more-help').removeClass('inactive');
  }
  
  function hideMoreHelp() {
    $('.more-help').addClass('inactive');
  }

  function scanForStyleQueueComments(text) {
    var comments = []
    var lines = text.split('\n');
    for (var i = 0; i < lines.length; ++i) {
      var parts = lines[i].match(/^([^:]+):(-?\d+):(.*)$/);
      if (!parts)
        continue;

      var file_name = parts[1];
      var line_number = parts[2];
      var comment_text = parts[3].trim();

      if (!file_name in files) {
        console.log('Filename in style queue output is not in the patch: ' + file_name);
        continue;
      }

      comments.push({
        'author': 'StyleQueue',
        'file_name': file_name,
        'line_number': line_number,
        'comment_text': comment_text
      });
    }
    return comments;
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

          var style_queue_comment_marker = 'Attachment ' + attachment_id + ' .details. did not pass style-queue.'
          if (text.match(style_queue_comment_marker))
            $.merge(comments, scanForStyleQueueComments(text));
        });
        displayPreviousComments(comments);
        ensureDraftCommentsDisplayed();
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
    if (!container)
      return 0;

    var from = fromLineNumber(container);
    var to = toLineNumber(container);
    return from || to;
  }

  function crawlDiff() {
    $('.Line').each(idify).each(hoverify);
    $('.FileDiff').each(function() {
      var header = $(this).children('h1');
      var url_hash = '#L' + firstLine(this);

      var file_name = header.text();
      files[file_name] = this;

      addExpandLinks(file_name);

      var diff_links = $('<div class="FileDiffLinkContainer LinkContainer">' +
          diffLinksHtml() +
          '</div>');

      var file_link = $('a', header)[0];
      // If the base directory in the file path does not match a WebKit top level directory,
      // then PrettyPatch.rb doesn't linkify the header.
      if (file_link) {
        file_link.target = "_blank";
        file_link.href += url_hash;
        diff_links.append(tracLinks(file_name, url_hash));
      }

      $('h1', this).after(diff_links);
      updateDiffLinkVisibility(this);
    });
  }

  function tracLinks(file_name, url_hash) {
    var trac_links = $('<a target="_blank">annotate</a><a target="_blank">revision log</a>');
    trac_links[0].href = 'http://trac.webkit.org/browser/trunk/' + file_name + '?annotate=blame' + url_hash;
    trac_links[1].href = 'http://trac.webkit.org/log/trunk/' + file_name;
    var implementation_suffix_list = ['.cpp', '.mm'];
    for (var i = 0; i < implementation_suffix_list.length; ++i) {
      var suffix = implementation_suffix_list[i];
      if (file_name.lastIndexOf(suffix) == file_name.length - suffix.length) {
        trac_links.prepend('<a target="_blank">header</a>');
        var stem = file_name.substr(0, file_name.length - suffix.length);
        trac_links[0].href= 'http://trac.webkit.org/log/trunk/' + stem + '.h';
      }
    }
    return trac_links;
  }

  function addExpandLinks(file_name) {
    if (file_name.indexOf('ChangeLog') != -1)
      return;

    var file_diff = files[file_name];

    // Don't show the links to expand upwards/downwards if the patch starts/ends without context
    // lines, i.e. starts/ends with add/remove lines.
    var first_line = file_diff.querySelector('.LineContainer:not(.context)');

    // If there is no element with a "Line" class, then this is an image diff.
    if (!first_line)
      return;

    var expand_bar_index = 0;
    if (!$(first_line).hasClass('add') && !$(first_line).hasClass('remove'))
      $('h1', file_diff).after(expandBarHtml(BELOW))

    $('br', file_diff).replaceWith(expandBarHtml());

    // jquery doesn't support :last-of-type, so use querySelector instead.
    var last_line = file_diff.querySelector('.LineContainer:last-of-type');
    // Some patches for new files somehow end up with an empty context line at the end
    // with a from line number of 0. Don't show expand links in that case either.
    if (!$(last_line).hasClass('add') && !$(last_line).hasClass('remove') && fromLineNumber(last_line) != 0)
      $(file_diff.querySelector('.DiffSection:last-of-type')).after(expandBarHtml(ABOVE));
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

  function removeContextBarBelow(expand_bar) {
    $('.context', expand_bar.nextElementSibling).detach();
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
      above_line_numbers = $('.Line:not(.context) .lineNumber', diff_section);
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
        below_line_numbers = $('.Line:not(.context) .lineNumber', diff_section);
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
      removeContextBarBelow(expand_bar);
    } else if (direction == ABOVE) {
      above_expansion.appendChild(lines);
    } else {
      below_expansion.insertBefore(lines, below_expansion.firstChild);
      removeContextBarBelow(expand_bar);
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
        '</span><span class="text"></span>' +
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
    var current_line = -1;
    var last_context_line = context[context.length - 1];
    if (patched_file[prev_line] == last_context_line)
      current_line = prev_line + 1;
    else {
      console.log('Hunk #' + hunk_num + ' FAILED.');
      return -1;
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
      var lines = $('.Line:not(.context)', section);
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

  var g_overallCommentsInputTimer;

  function handleOverallCommentsInput() {
    setAutoSaveStateIndicator('saving');
    // Save draft comments after we haven't received an input event in 1 second.
    if (g_overallCommentsInputTimer)
      clearTimeout(g_overallCommentsInputTimer);
    g_overallCommentsInputTimer = setTimeout(saveDraftComments, 1000);
  }

  function onBodyResize() {
    updateToolbarAnchorState();
  }

  function updateToolbarAnchorState() {
    // For iPad, we always leave the toolbar at the bottom of the document
    // because of the iPad's handling of position:fixed and scrolling.
    if (navigator.platform.indexOf("iPad") != -1)
      return;

    var toolbar = $('#toolbar');
    // Unanchor the toolbar and then see if it's bottom is below the body's bottom.
    toolbar.toggleClass('anchored', false);
    var toolbar_bottom = toolbar.offset().top + toolbar.outerHeight();
    var should_anchor = toolbar_bottom >= document.body.clientHeight;
    toolbar.toggleClass('anchored', should_anchor);
  }

  function diffLinksHtml() {
    return '<a href="javascript:" class="unify-link">unified</a>' +
      '<a href="javascript:" class="side-by-side-link">side-by-side</a>';
  }

  function appendToolbar() {
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
          '</span>' +
        '</div>' +
        '<div class="autosave-state"></div>' +
        '</div>');

    $('.overallComments textarea').bind('click', openOverallComments);
    $('.overallComments textarea').bind('input', handleOverallCommentsInput);
  }

  function handleDocumentReady() {
    crawlDiff();
    fetchHistory();
    $(document.body).prepend('<div id="message">' +
        '<div class="help">Select line numbers to add a comment. Scroll though diffs with the "j" and "k" keys.' +
          '<div class="DiffLinks LinkContainer">' + diffLinksHtml() + '</div>' +
          '<a href="javascript:" class="more">[more]</a>' +
          '<div class="more-help inactive">' +
            '<div class="winter"></div>' +
            '<div class="lightbox"><table>' +
              '<tr><td>enter</td><td>add/edit comment for focused item</td></tr>' +
              '<tr><td>escape</td><td>accept current comment / close preview and help popups</td></tr>' +
              '<tr><td>j</td><td>focus next diff</td></tr>' +
              '<tr><td>k</td><td>focus previous diff</td></tr>' +
              '<tr><td>shift + j</td><td>focus next line</td></tr>' +
              '<tr><td>shift + k</td><td>focus previous line</td></tr>' +
              '<tr><td>n</td><td>focus next comment</td></tr>' +
              '<tr><td>p</td><td>focus previous comment</td></tr>' +
              '<tr><td>r</td><td>focus review select element</td></tr>' +
              '<tr><td>ctrl + shift + up</td><td>extend context of the focused comment</td></tr>' +
              '<tr><td>ctrl + shift + down</td><td>shrink context of the focused comment</td></tr>' +
            '</table></div>' +
          '</div>' +
        '</div>' +
        '</div>');

    appendToolbar();

    $(document.body).prepend('<div id="comment_form" class="inactive"><div class="winter"></div><div class="lightbox"><iframe id="reviewform" src="attachment.cgi?id=' + attachment_id + '&action=reviewform"></iframe></div></div>');
    $('#reviewform').bind('load', handleReviewFormLoad);

    // Create a dummy iframe and monitor resizes in it's contentWindow to detect when the top document's body changes size.
    // FIXME: Should we setTimeout throttle these?
    var resize_iframe = $('<iframe class="pseudo_resize_event_iframe"></iframe>');
    $(document.body).append(resize_iframe);
    // Handle the event on a timeout to avoid crashing Firefox.
    $(resize_iframe[0].contentWindow).bind('resize', function() { setTimeout(onBodyResize, 0)});

    updateToolbarAnchorState();
    loadDiffState();
  };

  function handleReviewFormLoad() {
    var review_form_contents = $('#reviewform').contents();
    if (review_form_contents[0].querySelector('#form-controls #flags')) {
      review_form_contents.bind('keydown', function(e) {
        if (e.keyCode == KEY_CODE.escape)
          hideCommentForm();
      });

      // This is the intial load of the review form iframe.
      var form = review_form_contents.find('form')[0];
      form.addEventListener('submit', eraseDraftComments);
      form.target = '';
      return;
    }

    // Review form iframe have the publish button has been pressed.
    var email_sent_to = review_form_contents[0].querySelector('#bugzilla-body dl');
    // If the email_send_to DL is not in the tree that means the publish failed for some reason,
    // e.g., you're not logged in. Show the comment form to allow you to login.
    if (!email_sent_to) {
      showCommentForm();
      return;
    }

    eraseDraftComments();
    // FIXME: Once WebKit supports seamless iframes, we can just make the review-form
    // iframe fill the page instead of redirecting back to the bug.
    window.location.replace($('#toolbar .bugLink a').attr('href'));
  }
  
  function eraseDraftComments() {
    g_draftCommentSaver.erase();
  }

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

    $('.context', file_diff).each(function() {
      convertLine(diff_type, this);
    });

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

  function discardComment(comment_block) {
    var line_id = $(comment_block).find('textarea').attr('data-comment-for');
    var line = $('#' + line_id)
    $(comment_block).slideUp('fast', function() {
      $(this).remove();
      line.removeAttr('data-has-comment');
      trimCommentContextToBefore(line, line_id);
      saveDraftComments();
    });
  }

  function handleUnfreezeComment() {
    unfreezeComment(this);
  }

  function unfreezeComment(comment) {
    var unfrozen_comment = $(comment).prev();
    unfrozen_comment.show();
    $(comment).remove();
    unfrozen_comment.find('textarea')[0].focus();
  }

  function showFileDiffLinks() {
    $('.LinkContainer', this).each(function() { this.style.opacity = 1; });
  }

  function hideFileDiffLinks() {
    $('.LinkContainer', this).each(function() { this.style.opacity = 0; });
  }

  function handleDiscardComment() {
    discardComment($(this).parents('.comment'));
  }
  
  function handleAcceptComment() {
    acceptComment($(this).parents('.comment'));
  }
  
  function acceptComment(comment) {
    var frozen_comment = freezeComment($(comment));
    focusOn(frozen_comment);
    saveDraftComments();
    return frozen_comment;
  }

  $('.FileDiff').live('mouseenter', showFileDiffLinks);
  $('.FileDiff').live('mouseleave', hideFileDiffLinks);
  $('.side-by-side-link').live('click', handleSideBySideLinkClick);
  $('.unify-link').live('click', handleUnifyLinkClick);
  $('.ExpandLink').live('click', handleExpandLinkClick);
  $('.frozenComment').live('click', handleUnfreezeComment);
  $('.comment .discard').live('click', handleDiscardComment);
  $('.comment .ok').live('click', handleAcceptComment);
  $('.more').live('click', showMoreHelp);
  $('.more-help .winter').live('click', hideMoreHelp);

  function freezeComment(comment_block) {
    var comment_textarea = comment_block.find('textarea');
    if (comment_textarea.val().trim() == '') {
      discardComment(comment_block);
      return;
    }
    var line_id = comment_textarea.attr('data-comment-for');
    var line = $('#' + line_id)
    var frozen_comment = $('<div class="frozenComment"></div>').text(comment_textarea.val());
    findCommentBlockFor(line).hide().after(frozen_comment);
    return frozen_comment;
  }

  function focusOn(node, opt_is_backward) {
    if (node.length == 0)
      return;

    // Give a tabindex so the element can receive actual browser focus.
    // -1 makes the element focusable without actually putting in in the tab order.
    node.attr('tabindex', -1);
    node.focus();
    // Remove the tabindex on blur to avoid having the node be mouse-focusable.
    node.bind('blur', function() { node.removeAttr('tabindex'); });
    
    var node_top = node.offset().top;
    var is_top_offscreen = node_top <= $(document).scrollTop();
    
    var half_way_point = $(document).scrollTop() + window.innerHeight / 2;
    var is_top_past_halfway = opt_is_backward ? node_top < half_way_point : node_top > half_way_point;

    if (is_top_offscreen || is_top_past_halfway)
      $(document).scrollTop(node_top - window.innerHeight / 2);
  }

  function visibleNodeFilterFunction(is_backward) {
    var y = is_backward ? $('#toolbar')[0].offsetTop - 1 : 0;
    var x = window.innerWidth / 2;
    var reference_element = document.elementFromPoint(x, y);

    if (reference_element.nodeName == 'HTML' || reference_element.nodeName == 'BODY') {
      // In case we hit test a margin between file diffs, shift a fudge factor and try again.
      // FIXME: Is there a better way to do this?
      var file_diffs = $('.FileDiff');
      var first_diff = file_diffs.first();
      var second_diff = $(file_diffs[1]);
      var distance_between_file_diffs = second_diff.position().top - first_diff.position().top - first_diff.height();

      if (is_backward)
        y -= distance_between_file_diffs;
      else
        y += distance_between_file_diffs;

      reference_element = document.elementFromPoint(x, y);
    }

    if (reference_element.nodeName == 'HTML' || reference_element.nodeName == 'BODY')
      return null;
    
    return function(node) {
      var compare = reference_element.compareDocumentPosition(node[0]);
      if (is_backward)
        return compare & Node.DOCUMENT_POSITION_PRECEDING;
      return compare & Node.DOCUMENT_POSITION_FOLLOWING;
    }
  }

  function focusNext(filter, direction) {
    var focusable_nodes = $('a,.Line,.frozenComment,.previousComment,.DiffBlock,.overallComments').filter(function() {
      return !$(this).hasClass('DiffBlock') || $('.add,.remove', this).size();
    });

    var is_backward = direction == DIRECTION.BACKWARD;
    var index = focusable_nodes.index($(document.activeElement));
    
    var extra_filter = null;

    if (index == -1) {
      if (is_backward)
        index = focusable_nodes.length;
      extra_filter = visibleNodeFilterFunction(is_backward);
    }

    var offset = is_backward ? -1 : 1;
    var end = is_backward ? -1 : focusable_nodes.size();
    for (var i = index + offset; i != end; i = i + offset) {
      var node = $(focusable_nodes[i]);
      if (filter(node) && (!extra_filter || extra_filter(node))) {
        focusOn(node, is_backward);
        return true;
      }
    }
    return false;
  }

  var DIRECTION = {FORWARD: 1, BACKWARD: 2};

  function isComment(node) {
    return node.hasClass('frozenComment') || node.hasClass('previousComment') || node.hasClass('overallComments');
  }
  
  function isDiffBlock(node) {
    return node.hasClass('DiffBlock');
  }
  
  function isLine(node) {
    return node.hasClass('Line');
  }

  function commentTextareaForKeyTarget(key_target) {
    if (key_target.nodeName == 'TEXTAREA')
      return $(key_target);

    var comment_textarea = $(document.activeElement).prev().find('textarea');
    if (!comment_textarea.size())
      return null;
    return comment_textarea;
  }

  function extendCommentContextUp(key_target) {
    var comment_textarea = commentTextareaForKeyTarget(key_target);
    if (!comment_textarea)
      return;

    var comment_base_line = comment_textarea.attr('data-comment-for');
    var diff_section = diffSectionFor(comment_textarea);
    var lines = $('.Line', diff_section);
    for (var i = 0; i < lines.length - 1; i++) {
      if (hasDataCommentBaseLine(lines[i + 1], comment_base_line)) {
        addDataCommentBaseLine(lines[i], comment_base_line);
        break;
      }
    }
  }

  function shrinkCommentContextDown(key_target) {
    var comment_textarea = commentTextareaForKeyTarget(key_target);
    if (!comment_textarea)
      return;

    var comment_base_line = comment_textarea.attr('data-comment-for');
    var diff_section = diffSectionFor(comment_textarea);
    var lines = contextLinesFor(comment_base_line, diff_section);
    if (lines.size() > 1)
      removeDataCommentBaseLine(lines[0], comment_base_line);
  }

  function handleModifyContextKey(e) {
    var handled = false;

    if (e.shiftKey && e.ctrlKey) {
      switch (e.keyCode) {
      case KEY_CODE.up:
        extendCommentContextUp(e.target);
        handled = true;
        break;

      case KEY_CODE.down:
        shrinkCommentContextDown(e.target);
        handled = true;
        break;
      }
    }

    if (handled)
      e.preventDefault();

    return handled;
  }

  $('textarea').live('keydown', function(e) {
    if (handleModifyContextKey(e))
      return;

    if (e.keyCode == KEY_CODE.escape)
      handleEscapeKeyInTextarea(this);
  });

  $('body').live('keydown', function(e) {
    // FIXME: There's got to be a better way to avoid seeing these keypress
    // events.
    if (e.target.nodeName == 'TEXTAREA')
      return;

    // Don't want to override browser shortcuts like ctrl+r.
    if (e.metaKey || e.ctrlKey)
      return;

    if (handleModifyContextKey(e))
      return;

    var handled = false;
    switch (e.keyCode) {
    case KEY_CODE.r:
      $('.review select').focus();
      handled = true;
      break;

    case KEY_CODE.n:
      handled = focusNext(isComment, DIRECTION.FORWARD);
      break;

    case KEY_CODE.p:
      handled = focusNext(isComment, DIRECTION.BACKWARD);
      break;

    case KEY_CODE.j:
      if (e.shiftKey)
        handled = focusNext(isLine, DIRECTION.FORWARD);
      else
        handled = focusNext(isDiffBlock, DIRECTION.FORWARD);
      break;

    case KEY_CODE.k:
      if (e.shiftKey)
        handled = focusNext(isLine, DIRECTION.BACKWARD);
      else
        handled = focusNext(isDiffBlock, DIRECTION.BACKWARD);
      break;
      
    case KEY_CODE.enter:
      handled = handleEnterKey();
      break;
      
    case KEY_CODE.escape:
      hideMoreHelp();
      handled = true;
      break;
    }
    
    if (handled)
      e.preventDefault();
  });
  
  function handleEscapeKeyInTextarea(textarea) {
    var comment = $(textarea).parents('.comment');
    if (comment.size())
      acceptComment(comment);

    textarea.blur();
    document.body.focus();
  }
  
  function handleEnterKey() {
    if (document.activeElement.nodeName == 'BODY')
      return;

    var focused = $(document.activeElement);

    if (focused.hasClass('frozenComment')) {
      unfreezeComment(focused);
      return true;
    }
    
    if (focused.hasClass('overallComments')) {
      openOverallComments();
      focused.find('textarea')[0].focus();
      return true;
    }
    
    if (focused.hasClass('previousComment')) {
      addCommentField(focused);
      return true;
    }

    var lines = focused.hasClass('Line') ? focused : $('.Line', focused);
    var last = lines.last();
    if (last.attr('data-has-comment')) {
      unfreezeCommentFor(last);
      return true;
    }

    addCommentForLines(lines);
    return true;
  }

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

      if (!$('[data-comment-for=' + comment_base_line + ']').length)
        removeDataCommentBaseLine(this, comment_base_line);
    });
  }

  var drag_select_start_index = -1;

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

  $(document.body).bind('mouseup', processSelectedLines);

  $('.lineNumber').live('click', function(e) {
    var line = lineFromLineDescendant($(this));
    if (line.hasClass('commentContext'))
      trimCommentContextToBefore(previousLineFor(line), line.attr('data-comment-base-line'));
    else if (e.shiftKey)
      extendCommentContextTo(line);
  }).live('mousedown', function(e) {
    // preventDefault to avoid selecting text when dragging to select comment context lines.
    // FIXME: should we use user-modify CSS instead?
    e.preventDefault();
    if (e.shiftKey)
      return;

    var line = lineFromLineDescendant($(this));
    drag_select_start_index = numberFrom(line.attr('id'));
    line.addClass('selected');
  });

  $('.LineContainer').live('mouseenter', function(e) {
    if (drag_select_start_index == -1 || e.shiftKey)
      return;
    selectToLineContainer(this);
  }).live('mouseup', function(e) {
    if (drag_select_start_index == -1 || e.shiftKey)
      return;

    selectToLineContainer(this);
    processSelectedLines();
  });

  function extendCommentContextTo(line) {
    var diff_section = diffSectionFor(line);
    var lines = $('.Line', diff_section);
    var lines_to_modify = [];
    var have_seen_start_line = false;
    var data_comment_base_line = null;
    lines.each(function() {
      if (data_comment_base_line)
        return;

      have_seen_start_line = have_seen_start_line || this == line[0];
      
      if (have_seen_start_line) {
        if ($(this).hasClass('commentContext'))
          data_comment_base_line = $(this).attr('data-comment-base-line');
        else
          lines_to_modify.push(this);
      }
    });
    
    // There is no comment context to extend.
    if (!data_comment_base_line)
      return;
    
    $(lines_to_modify).each(function() {
      $(this).addClass('commentContext');
      $(this).attr('data-comment-base-line', data_comment_base_line);
    });
  }

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

    // Ensure that the selected lines are all contained in the same DiffSection.
    var selected_lines = $('.selected');
    var selected_diff_section = diffSectionFor(selected_lines.first());
    var new_diff_section = diffSectionFor(line);
    if (new_diff_section[0] != selected_diff_section[0]) {
      var lines = $('.Line', selected_diff_section);
      if (numberFrom(selected_lines.first().attr('id')) == drag_select_start_index)
        line = lines.last();
      else
        line = lines.first();
    }
    
    selectTo(numberFrom(line.attr('id')));
  }

  function processSelectedLines() {
    drag_select_start_index = -1;
    addCommentForLines($('.selected'));
  }
  
  function addCommentForLines(lines) {    
    if (!lines.size())
      return;

    var already_has_comment = lines.last().hasClass('commentContext');

    var comment_base_line;
    if (already_has_comment)
      comment_base_line = lines.last().attr('data-comment-base-line');
    else {
      var last = lineFromLineDescendant(lines.last());
      addCommentFor($(last));
      comment_base_line = last.attr('id');
    }

    lines.each(function() {
      addDataCommentBaseLine(this, comment_base_line);
      $(this).removeClass('selected');
    });

    saveDraftComments();
  }

  function hasDataCommentBaseLine(line, id) {
    var val = $(line).attr('data-comment-base-line');
    if (!val)
      return false;

    var parts = val.split(' ');
    for (var i = 0; i < parts.length; i++) {
      if (parts[i] == id)
        return true;
    }
    return false;
  }

  function addDataCommentBaseLine(line, id) {
    $(line).addClass('commentContext');
    if (hasDataCommentBaseLine(line, id))
      return;

    var val = $(line).attr('data-comment-base-line');
    var parts = val ? val.split(' ') : [];
    parts.push(id);
    $(line).attr('data-comment-base-line', parts.join(' '));
  }

  function removeDataCommentBaseLine(line, comment_base_lines) {
    var val = $(line).attr('data-comment-base-line');
    if (!val)
      return;

    var base_lines = comment_base_lines.split(' ');
    var parts = val.split(' ');
    var new_parts = [];
    for (var i = 0; i < parts.length; i++) {
      if (base_lines.indexOf(parts[i]) == -1)
        new_parts.push(parts[i]);
    }

    var new_comment_base_line = new_parts.join(' ');
    if (new_comment_base_line)
      $(line).attr('data-comment-base-line', new_comment_base_line);
    else {
      $(line).removeAttr('data-comment-base-line');
      $(line).removeClass('commentContext');
    }
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

  $('#comment_form .winter').live('click', hideCommentForm);

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

  function showCommentForm() {
    $('#comment_form').removeClass('inactive');
    $('#reviewform').contents().find('#submitBtn').focus();
  }
  
  function hideCommentForm() {
    $('#comment_form').addClass('inactive');
    
    // Make sure the top document has focus so key events don't keep going to the review form.
    document.body.tabIndex = -1;
    document.body.focus();
  }

  $('#preview_comments').live('click', function() {
    fillInReviewForm();
    showCommentForm();
  });

  $('#post_comments').live('click', function() {
    fillInReviewForm();
    $('#reviewform').contents().find('form').submit();
  });
  
  if (CODE_REVIEW_UNITTEST) {
    window.DraftCommentSaver = DraftCommentSaver;
    window.addPreviousComment = addPreviousComment;
    window.tracLinks = tracLinks;
    window.crawlDiff = crawlDiff;
    window.discardComment = discardComment;
    window.addCommentField = addCommentField;
    window.acceptComment = acceptComment;
    window.appendToolbar = appendToolbar;
    window.eraseDraftComments = eraseDraftComments;
    window.unfreezeComment = unfreezeComment;
    window.g_draftCommentSaver = g_draftCommentSaver;
  } else {
    $(document).ready(handleDocumentReady)
  }
})();
