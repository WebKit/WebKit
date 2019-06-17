<?php

require_once('../include/json-header.php');
require_once('../include/commit-log-fetcher.php');

function main($paths) {
    if (count($paths) < 1 || count($paths) > 2)
        exit_with_error('InvalidRequest');

    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $fetcher = new CommitLogFetcher($db);

    if (!is_numeric($paths[0])) {
        $repository_id = $fetcher->repository_id_from_name($paths[0]);
        if (!$repository_id)
            exit_with_error('RepositoryNotFound', array('repositoryName' => $paths[0]));
    } else
        $repository_id = intval($paths[0]);

    $filter = array_get($paths, 1);
    $commits = array();
    if (!$filter) {
        $keyword = array_get($_GET, 'keyword'); // V2 UI compatibility.
        $preceding_revision = array_get($_GET, 'precedingRevision');
        $last_revision = array_get($_GET, 'lastRevision');
        $commits = $fetcher->fetch_between($repository_id, $preceding_revision, $last_revision, $keyword);
    } else if ($filter == 'oldest') {
        $commits = $fetcher->fetch_oldest($repository_id);
    } else if ($filter == 'latest') {
        $platform_id = array_get($_GET, 'platform');
        if ($platform_id) {
            if (!is_numeric($platform_id))
                exit_with_error('InvalidPlatform', array('platform' => $platform_id));
            $platform_id = intval($platform_id);
            $commits = $fetcher->fetch_latest_for_platform($repository_id, $platform_id);
        } else
            $commits = $fetcher->fetch_latest($repository_id);
    } else if ($filter == 'owned-commits') {
        $owner_revision = array_get($_GET, 'owner-revision');
        $commits = $fetcher->fetch_owned_commits_for_revision($repository_id, $owner_revision);
    } else if ($filter == 'last-reported') {
        $from = array_get($_GET, 'from');
        $to = array_get($_GET, 'to');
        if ($from && $to)
            $commits = $fetcher->fetch_last_reported_between_orders($repository_id, $from, $to);
        else
            $commits = $fetcher->fetch_last_reported($repository_id);
    } else {
        $prefix_match = $keyword = array_get($_GET, 'prefix-match');
        $commits = $fetcher->fetch_revision($repository_id, $filter, $prefix_match);
    }

    if (!is_array($commits))
        exit_with_error('FailedToFetchCommits', array('repository' => $repository_id, 'filter' => $filter));

    exit_with_success(array('commits' => $commits));
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
