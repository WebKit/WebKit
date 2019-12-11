<!DOCTYPE html>

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en" debug="true">
<head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
	
	<script type="text/javascript" src="../frameworks/<?php echo $_GET['include']; ?>"></script>
	
	<script type="text/javascript">
		
		var modifier = <?php echo '"'.$_GET['modifier'].'"'; ?>;
	
		function test(selector){
			try {
				var start = new Date().getTime();
				var elements = <?php echo $_GET['function']; ?>(selector);
				var step = (new Date().getTime() - start);
				if (step > 750) return {'time': step, 'found': elements.length};
				for(var i = 0; i < 250; i++)
				{
				<?php echo $_GET['function']; ?>(selector);
				}
				var end = (new Date().getTime() - start);
				return {'time': Math.round(end), 'found': elements.length};
			} catch(err){
				if (elements == undefined) elements = {length: -1};
				return ({'time': (new Date().getTime() - start), 'found': elements.length, 'error': err});
			}

		};
	
	</script>
	
</head>

<body>
	
	<?php include('../template.html');?>

</body>
</html>
