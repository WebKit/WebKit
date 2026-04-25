var panel_obj = document.getElementById("content_panel");
if (panel_obj) {
  var encoded_content = "≤‚ ‘";
  if (encoded_content == panel_obj.innerHTML)
    panel_obj.innerHTML = "SUCCESS";
  else
    panel_obj.innerHTML = "FAILURE";
  if (window.testRunner)
    testRunner.notifyDone();
}
