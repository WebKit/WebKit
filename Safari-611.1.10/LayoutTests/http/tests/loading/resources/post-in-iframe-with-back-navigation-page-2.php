<!DOCTYPE html>
<h1>Page 2</h1>
<span id="submissionTime"><?php print microtime();?></span><br/>
<form action="./post-in-iframe-with-back-navigation-page-3.php" name="form" method="POST">
</form>
<a id="link" href="javascript:document.form.submit();">to page 3</a><br/>
<a id="backLink" href="javascript:history.back();">go back</a>
