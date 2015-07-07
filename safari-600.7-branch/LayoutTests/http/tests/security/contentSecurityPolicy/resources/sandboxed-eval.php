<?php
header("Content-Security-Policy: sandbox allow-scripts");
?>
<script>
alert('PASS (1/2): Script can execute');
</script>
<script>
eval("alert('PASS (2/2): Eval works')");
</script>
Done.
