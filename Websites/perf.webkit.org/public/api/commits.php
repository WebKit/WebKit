<?php

require_once('../include/json-header.php');

function main($paths) {
    if (count($paths) < 1 || count($paths) > 2)
        exit_with_error('InvalidRequest');

    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $repository_name = $paths[0];
    $repository_row = $db->select_first_row('repositories', 'repository', array('name' => $repository_name));
    if (!$repository_row)
        exit_with_error('RepositoryNotFound', array('repositoryName' => $repository_name));
    $repository_id = $repository_row['repository_id'];

    $filter = array_get($paths, 1);
    $single_commit = NULL;
    $commits = array();
    if (!$filter) {
        $commits = $db->fetch_table('commits', 'commit_time');
    } else if ($filter == 'oldest') {
        $single_commit = $db->select_first_row('commits', 'commit', array('repository' => $repository_id), 'time');
    } else if ($filter == 'latest') {
        $single_commit = $db->select_last_row('commits', 'commit', array('repository' => $repository_id), 'time');
    } else if ($filter == 'last-reported') {
        $single_commit = $db->select_last_row('commits', 'commit', array('repository' => $repository_id, 'reported' => true), 'time');
    } else if (ctype_alnum($filter)) {
        $single_commit = commit_from_revision($db, $repository_id, $repository_name, $filter);
    } else {
        $matches = array();
        if (!preg_match('/([A-Za-z0-9]+)[\:\-]([A-Za-z0-9]+)/', $filter, $matches))
            exit_with_error('UnknownFilter', array('repositoryName' => $repository_name, 'filter' => $filter));

        $first = commit_from_revision($db, $repository_id, $matches[1])['commit_time'];
        $second = commit_from_revision($db, $repository_id, $matches[2])['commit_time'];
        $in_order = $first < $second;

        $commits = fetch_commits_between($db, $repository_id, $in_order ? $first : $second, $in_order ? $second : $first);
    }

    exit_with_success(array('commits' => format_commits($single_commit ? array($single_commit) : $commits)));
}

function commit_from_revision($db, $repository_id, $revision) {
    $all_but_first = substr($revision, 1);
    if ($revision[0] == 'r' && ctype_digit($all_but_first))
        $revision = $all_but_first;
    $commit_info = array('repository' => $repository_id, 'revision' => $revision);
    $row = $db->select_last_row('commits', 'commit', $commit_info);
    if (!$row)
        exit_with_error('UnknownCommit', $commit_info);
    return $row;
}

function fetch_commits_between($db, $repository_id, $from, $to) {
    $commits = $db->query_and_fetch_all('SELECT * FROM commits
        WHERE commit_repository = $1 AND commit_time >= $2 AND commit_time <= $3 AND commit_reported = true ORDER BY commit_time',
        array($repository_id, $from, $to));
    if (!$commits)
        exit_with_error('FailedToFetchCommits', array('repository' => $repository_id, 'from' => $from, 'to' => $to));
    return $commits;
}

function format_commits($commits) {
    $formatted_commits = array();
    foreach ($commits as $commit_row) {
        array_push($formatted_commits, array(
            'id' => $commit_row['commit_id'],
            'revision' => $commit_row['commit_revision'],
            'parent' => $commit_row['commit_parent'],
            'time' => $commit_row['commit_time'],
            'author' => array('name' => $commit_row['commit_author_name'], 'email' => $commit_row['commit_author_email']),
            'message' => $commit_row['commit_message']
        ));
    }
    return $formatted_commits;
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
