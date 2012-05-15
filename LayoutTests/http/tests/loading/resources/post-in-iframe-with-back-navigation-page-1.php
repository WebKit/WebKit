<!DOCTYPE html>
<h1>Page 1</h1>
<span id="submissionTime"><?php print microtime();?></span><br/>
<form action="./post-in-iframe-with-back-navigation-page-2.php" name="form" method="POST">
</form>
<a id="link" href="javascript:document.form.submit();">to page 2</a>
