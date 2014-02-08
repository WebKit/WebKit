<?php

require_once('../include/admin-header.php');
require_once('../include/manifest.php');

if ($db) {
    if (regenerate_manifest())
        notice("Regenerated the manifest");
}

require('../include/admin-footer.php');

?>
