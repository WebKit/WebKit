<?php
header("Content-Security-Policy: sandbox allow-scripts allow-modals");
?>
<script>
console.log('PASS (1/2): Script can execute');
</script>
<script>
eval("console.log('PASS (2/2): Eval works')");
</script>
Done.
