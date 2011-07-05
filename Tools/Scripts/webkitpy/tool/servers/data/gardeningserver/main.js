(function() {
  function quit() {
    $.post('/quitquitquit', function(data){
      $('.butterbar .status').html(data)
      $('.butterbar').fadeIn();
    });
  }

  function hide() {
    $(this).parent().fadeOut();
  }

  $('.hide').live('click', hide);
  $('.quit').live('click', quit);

  $(document).ready(function() {
    $('.butterbar').fadeOut();
  })
})();
