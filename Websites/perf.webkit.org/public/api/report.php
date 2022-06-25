<?php

require_once('../include/json-header.php');
require_once('../include/manifest-generator.php');
require_once('../include/report-processor.php');

function main($post_data) {
    set_exit_detail('failureStored', false);

    $maintenance_mode = config('maintenanceMode');
    if ($maintenance_mode && !config('maintenanceDirectory'))
        exit_with_error('MaintenanceDirectoryNotSet');

    $db = new Database;
    if (!$maintenance_mode && !$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    // Convert all floating points to strings to avoid parsing them in PHP.
    // FIXME: Do this conversion in the submission scripts themselves.
    $parsed_json = json_decode(preg_replace('/(?<=[\s,\[])(\d+(\.\d+)?)(\s*[,\]])/', '"$1"$3', $post_data), true);
    if (!$parsed_json)
        exit_with_error('FailedToParseJSON');

    set_exit_detail('processedRuns', 0);
    foreach ($parsed_json as $i => $report) {
        if (!$maintenance_mode) {
            $processor = new ReportProcessor($db);
            $processor->process($report);
        }
        set_exit_detail('processedRuns', $i + 1);
    }

    if ($maintenance_mode) {
        $files = scandir(config_path('maintenanceDirectory', ''));
        $i = 0;
        $filename = '';
        do {
            $i++;
            $filename = "$i.json";
        } while (in_array($filename, $files));
        file_put_contents(config_path('maintenanceDirectory', $filename), $post_data, LOCK_EX);
    } else {
        $generator = new ManifestGenerator($db);
        if (!$generator->generate())
            exit_with_error('FailedToGenerateManifest');
        else if (!$generator->store())
            exit_with_error('FailedToStoreManifest');
    }

    exit_with_success();
}

main(file_get_contents('php://input'));

?>
