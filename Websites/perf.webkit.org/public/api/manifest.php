<?php

require_once('../include/json-header.php');
require_once('../include/manifest-generator.php');

function main() {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $generator = new ManifestGenerator($db);
    if (!$generator->generate())
        exit_with_error('FailedToGenerateManifest');

    if (!$generator->store())
        exit_with_error('FailedToStoreManifest');

    exit_with_success($generator->manifest());
}

main();

?>
