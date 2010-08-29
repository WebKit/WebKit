(function() {
  var kDeleteImage = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAMAAAAoLQ9TAAAAA3NCSVQICAjb4U/gAAAAn1BMVEX////4YmT/dnnyTE//dnn9bnH6am34YmT3XWD2WVv2VVjsOj3oMDLlJyrjICL2VVjzUVTwR0ruPT/iHB72WVvwR0rzUVT/h4r/gob/foH/eXv/dnn/cnT9bnH/bG76am3/Zmb6ZGf4YmT3XWD/WFv2WVv/VFf2VVj0TVDyTE/2SkzwR0rvREfuQUPuPT/sOj3rNDboMDLnLTDlJyrjICIhCpwnAAAANXRSTlMAESIiMzMzMzMzMzMzMzNERERERHd3qv///////////////////////////////////////0mgXpwAAAAJcEhZcwAAHngAAB54AcurAx8AAAAYdEVYdFNvZnR3YXJlAEFkb2JlIEZpcmV3b3Jrc0+zH04AAACVSURBVBiVbczXFoIwDAbguHGi4mqbWugQZInj/Z9NSuXAhblJvuTkB+jV4NeHY9e9g+/M2KSxFKdRY0JwWltxoo72gvRMxcxTgqrM/Qp2QWmdt+kRJ5SyzgCGao09zw3TN8yWnSNEfo3LVWdTPJIwqdbWCyN5XABUeZi+NvViG0trgHeRPgM77O6l+/04A+zb9AD+1Bf6lg3jQQJJTgAAAABJRU5ErkJggg==';

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

  var previous_comments = [];
  var current_comment = {}
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

  function findCommentPositionFor(line) {
    var position = line;
    while (position.next() && position.next().hasClass('previous_comment'))
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

  function addCommentField() {
    var id = $(this).attr('data-comment-for');
    if (!id)
      id = this.id;
    var line = $('#' + id);
    if (line.attr('data-has-comment'))
      return;
    line.attr('data-has-comment', 'true');
    var comment_block = $('<div class="comment"><div class="actions"><img class="delete" src="' + kDeleteImage + '"></div><textarea data-comment-for="' + id + '"></textarea></div>');
    insertCommentFor(line, comment_block);
    comment_block.each(hoverify);
    comment_block.children('textarea').focus();
  }

  function displayPreviousComments() {
    comment_collection = this;
    forEachLine(function(line) {
      var line_id = line.attr('id');
      var comment = comment_collection[line_id];
      if (comment) {
        var comment_block = $('<div data-comment-for="' + line_id + '" class="previous_comment"></div>');
        comment_block.text(comment).prepend('<div class="reply">Click to reply</div>');
        comment_block.each(hoverify).click(addCommentField);
        insertCommentFor(line, comment_block);
      }
    });
  }

  $(document).ready(function() {
    $('.Line').each(idify).each(hoverify).click(addCommentField);
    $(previous_comments).each(displayPreviousComments);
    $(document.body).prepend('<div id="toolbar"><button id="post_comments">Prepare comments</button></div>');
    $(document.body).prepend('<div id="comment_form" class="inactive"><div class="winter"></div><div class="lightbox"><iframe src="attachment.cgi?id=' + attachment_id + '&action=reviewform"></iframe></div></div>');
  });

  $('.comment textarea').live('keydown', function() {
    var line_id = $(this).attr('data-comment-for');
    current_comment[line_id] = $(this).val();
  });

  $('.comment .delete').live('click', function() {
    var line_id = $(this).parentsUntil('.comment').next().attr('data-comment-for');
    delete current_comment[line_id];
    var line = $('#' + line_id)
    findCommentBlockFor(line).remove();
    line.removeAttr('data-has-comment');
  });

  function contextSnippetFor(line) {
    var file_name = line.parent().prev().text();
    var line_number = line.hasClass('remove') ? line.children('.from').text() : line.children('.to').text();

    var action = ' ';
    if (line.hasClass('add'))
      action = '+';
    else if (line.hasClass('remove'))
      action = '-';

    var text = line.children('.text').text();
    return '> ' + file_name + ':' + line_number + '\n> ' + action + text;
  }

  $('#comment_form .winter').live('click', function() {
    $('#comment_form').addClass('inactive');
  });

  $('#post_comments').live('click', function() {
    var comments_in_context = []
    forEachLine(function(line) {
      if (line.attr('data-has-comment') != 'true')
        return;
      var snippet = contextSnippetFor(line);
      var comment = findCommentBlockFor(line).children('textarea').val();
      if (comment == '')
        return;
      comments_in_context.push(snippet + '\n' + comment);
    });
    $('#comment_form').removeClass('inactive');
    var comment = comments_in_context.join('\n\n');
    console.log(comment);
    $('#comment_form').find('iframe').contents().find('#comment').val(comment);
  });
})();
