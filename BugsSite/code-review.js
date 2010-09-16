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
  function determineAttachmentID() {
    try {
      return /id=(\d+)/.exec(window.location.search)[1]
    } catch (ex) {
      return;
    }
  }

  // Attempt to activate only in the "Formatted Diff" context.
  if (window.top != window)
    return;
  var attachment_id = determineAttachmentID();
  if (!attachment_id)
    return;

  var next_line_id = 0;

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

  function previousCommentsFor(line) {
    var comments = [];
    var position = line;
    while (position.next() && position.next().hasClass('previousComment')) {
      position = position.next();
      comments.push(position.get());
    }
    return $(comments);
  }

  function findCommentPositionFor(line) {
    var position = line;
    while (position.next() && position.next().hasClass('previousComment'))
      position = position.next();
    return position;
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

    var comment_block = $('<div class="comment"><textarea data-comment-for="' + line.attr('id') + '"></textarea><div class="actions"><button class="ok">Ok</button><button class="discard">Discard</button></div></div>');
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

  var files = {}

  function addPreviousComment(line, author, comment_text) {
    var comment_block = $('<div data-comment-for="' + line.attr('id') + '" class="previousComment"></div>');
    var author_block = $('<div class="author"></div>').text(author + ':');
    var text_block = $('<div class="content"></div>').text(comment_text);
    comment_block.append(author_block).append(text_block).each(hoverify).click(addCommentField);
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
    if (comments.length == 0)
      return;
    descriptor = comments.length + ' comment';
    if (comments.length > 1)
      descriptor += 's';
    $('#toolbar .commentStatus').text('This patch has ' + descriptor + '.  Scroll through them with the "n" and "p" keys.');
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
    $('#toolbar .actions').append(
      $('<span class="review"> r: ' + flag_control + '</span>')).append(
      $('<span class="commitQueue"> cq: ' + flag_control + '</span>'));

    details.find('#flags select').each(function() {
      findControlForFlag(this).attr('selectedIndex', $(this).attr('selectedIndex'));
    });
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
      $('#toolbar .actions').append($('<iframe class="statusBubble" src="https://webkit-commit-queue.appspot.com/status-bubble/' + attachment_id + '" scrolling="no"></iframe>'));
    });
  }

  function crawlDiff() {
    $('.Line').each(idify).each(hoverify).dblclick(addCommentField);
    $('.FileDiff').each(function() {
      var file_name = $(this).children('h1').text();
      files[file_name] = this;
    });
  }

  $(document).ready(function() {
    crawlDiff();
    fetchHistory();
    $(document.body).prepend('<div id="toolbar"><div class="actions"><button id="post_comments">Publish</button></div><div class="message"><span class="commentStatus"></span> <span class="help">Double-click a line to add a comment.</span></div></div>');
    $(document.body).prepend('<div id="comment_form" class="inactive"><div class="winter"></div><div class="lightbox"><iframe id="reviewform" src="attachment.cgi?id=' + attachment_id + '&action=reviewform"></iframe></div></div>');
    $(document.body).append('<div class="overallComments"><div class="description">Overall comments:</div><textarea></textarea></div>');
  });

  function discardComment() {
    var line_id = $(this).parentsUntil('.comment').parent().find('textarea').attr('data-comment-for');
    var line = $('#' + line_id)
    findCommentBlockFor(line).slideUp('fast', function() {
      $(this).remove();
      line.removeAttr('data-has-comment');
      trimCommentContextToBefore(line);
    });
  }

  function unfreezeComment() {
    $(this).prev().show();
    $(this).remove();
  }

  $('.comment .discard').live('click', discardComment);

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

  $('.frozenComment').live('click', unfreezeComment);

  function focusOn(comment) {
    $('.focused').removeClass('focused');
    if (comment.length == 0)
      return;
    $(document).scrollTop(comment.addClass('focused').position().top - window.innerHeight/2);
  }

  function focusNextComment() {
    var comments = $('.previousComment');
    if (comments.length == 0)
      return;
    var index = comments.index($('.focused'));
    // Notice that -1 gets mapped to 0.
    focusOn($(comments.get(index + 1)));
  }

  function focusPreviousComment() {
    var comments = $('.previousComment');
    if (comments.length == 0)
      return;
    var index = comments.index($('.focused'));
    if (index == -1)
      index = comments.length;
    if (index == 0) {
      focusOn([]);
      return;
    }
    focusOn($(comments.get(index - 1)));
  }

  var kCharCodeForN = 'n'.charCodeAt(0);
  var kCharCodeForP = 'p'.charCodeAt(0);

  $('body').live('keypress', function() {
    // FIXME: There's got to be a better way to avoid seeing these keypress
    // events.
    if (event.target.nodeName == 'TEXTAREA')
      return;
    if (event.charCode == kCharCodeForN)
      focusNextComment();
    else if (event.charCode == kCharCodeForP)
      focusPreviousComment();
  });

  function contextLinesFor(line) {
    var context = [];
    while (line.hasClass('commentContext')) {
      $.merge(context, line);
      line = line.prev();
    }
    return $(context.reverse());
  }

  function trimCommentContextToBefore(line) {
    while (line.hasClass('commentContext') && line.attr('data-has-comment') != 'true') {
      line.removeClass('commentContext');
      line = line.prev();
    }
  }

  var in_drag_select = false;

  function stopDragSelect() {
    $('.selected').removeClass('selected');
    in_drag_select = false;
  }

  $('.lineNumber').live('click', function() {
    var line = $(this).parent();
    if (line.hasClass('commentContext'))
      trimCommentContextToBefore(line.prev());
  }).live('mousedown', function() {
    in_drag_select = true;
    $(this).parent().addClass('selected');
    event.preventDefault();
  });
  
  $('.Line').live('mouseenter', function() {
    if (!in_drag_select)
      return;

    var before = $(this).prevUntil('.selected')
    if (before.prev().hasClass('selected'))
      before.addClass('selected');

    var after = $(this).nextUntil('.selected')
    if (after.next().hasClass('selected'))
      after.addClass('selected');

    $(this).addClass('selected');
  }).live('mouseup', function() {
    if (!in_drag_select)
      return;
    var selected = $('.selected');
    var should_add_comment = !selected.last().next().hasClass('commentContext');
    selected.addClass('commentContext');
    if (should_add_comment)
      addCommentFor(selected.last());
  });

  $('.DiffSection').live('mouseleave', stopDragSelect).live('mouseup', stopDragSelect);

  function contextSnippetFor(line, indent) {
    var snippets = []
    contextLinesFor(line).each(function() {
      var action = ' ';
      if ($(this).hasClass('add'))
        action = '+';
      else if ($(this).hasClass('remove'))
        action = '-';
      var text = $(this).children('.text').text();
      snippets.push(indent + action + text);
    });
    return snippets.join('\n');
  }

  function fileNameFor(line) {
    return line.parentsUntil('.FileDiff').parent().find('h1').text();
  }

  function indentFor(depth) {
    return (new Array(depth + 1)).join('>') + ' ';
  }

  function snippetFor(line, indent) {
    var file_name = fileNameFor(line);
    var line_number = line.hasClass('remove') ? '-' + line.children('.from').text() : line.children('.to').text();
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

  $('#post_comments').live('click', function() {
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
    $('#comment_form').removeClass('inactive');
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
  });
})();
