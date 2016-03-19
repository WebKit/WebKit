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
        $keyword = array_get($_GET, 'keyword');
        $from = array_get($_GET, 'from');
        $to = array_get($_GET, 'to');
        $commits = $fetcher->fetch_between($repository_id, $from, $to, $keyword);
    } else if ($filter == 'oldest') {
        $commits = $fetcher->fetch_oldest($repository_id);
    } else if ($filter == 'latest') {
        $commits = $fetcher->fetch_latest($repository_id);
    } else if ($filter == 'last-reported') {
        $commits = $fetcher->fetch_last_reported($repository_id);
    } else if (ctype_alnum($filter)) {
        $commits = $fetcher->fetch_revision($repository_id, $filter);
    } else {
        $matches = array();
        if (!preg_match('/([A-Za-z0-9]+)[\:\-]([A-Za-z0-9]+)/', $filter, $matches))
            exit_with_error('UnknownFilter', array('repositoryName' => $repository_name, 'filter' => $filter));
        $commits = $fetcher->fetch_between($repository_id, $matches[1], $matches[2]);
    }

    if (!is_array($commits))
        exit_with_error('FailedToFetchCommits', array('repository' => $repository_id, 'filter' => $filter));

    exit_with_success(array('commits' => $commits));
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
