<?php

require_once('../include/json-header.php');
require_once('../include/manifest.php');
require_once('../include/report-processor.php');

$db = new Database;
if (!$db->connect())
    exit_with_error('DatabaseConnectionFailure');

$report_id = array_get($_POST, 'report');
if (!$report_id)
    $report_id = array_get($_GET, 'report');
$report_id = intval($report_id);
if (!$report_id)
    exit_with_error('ReportIdNotSpecified');

$report_row = $db->select_first_row('reports', 'report', array('id' => $report_id));
if (!$report_row)
    return exit_with_error('ReportNotFound', array('reportId', $report_id));

$processor = new ReportProcessor($db);
$processor->process(json_decode($report_row['report_content'], true), $report_id);

$generator = new ManifestGenerator($db);
if (!$generator->generate())
    exit_with_error('FailedToGenerateManifest');
else if (!$generator->store())
    exit_with_error('FailedToStoreManifest');

exit_with_success();

?>
