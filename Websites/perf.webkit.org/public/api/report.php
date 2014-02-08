<?php

require_once('../include/json-header.php');
require_once('../include/manifest.php');
require_once('../include/report-processor.php');

function main($post_data) {
    set_exit_detail('failureStored', false);

    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    // Convert all floating points to strings to avoid parsing them in PHP.
    // FIXME: Do this conversion in the submission scripts themselves.
    $parsed_json = json_decode(preg_replace('/(?<=[\s,\[])(\d+(\.\d+)?)(\s*[,\]])/', '"$1"$3', $post_data), true);
    if (!$parsed_json)
        exit_with_error('FailedToParseJSON');

    set_exit_detail('processedRuns', 0);
    foreach ($parsed_json as $i => $report) {
        $processor = new ReportProcessor($db);
        $processor->process($report);
        set_exit_detail('processedRuns', $i + 1);
    }

    $generator = new ManifestGenerator($db);
    if (!$generator->generate())
        exit_with_error('FailedToGenerateManifest');
    else if (!$generator->store())
        exit_with_error('FailedToStoreManifest');

    exit_with_success();
}

main($HTTP_RAW_POST_DATA);

?>
