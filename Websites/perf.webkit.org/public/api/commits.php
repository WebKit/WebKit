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
        $keyword = array_get($_GET, 'keyword');
        $from = array_get($_GET, 'from');
        $to = array_get($_GET, 'to');
        $commits = fetch_commits_between($db, $repository_id, $from, $to, $keyword);
    } else if ($filter == 'oldest') {
        $single_commit = $db->select_first_row('commits', 'commit', array('repository' => $repository_id), 'time');
    } else if ($filter == 'latest') {
        $single_commit = $db->select_last_row('commits', 'commit', array('repository' => $repository_id), 'time');
    } else if ($filter == 'last-reported') {
        $single_commit = $db->select_last_row('commits', 'commit', array('repository' => $repository_id, 'reported' => true), 'time');
    } else if (ctype_alnum($filter)) {
        $single_commit = commit_from_revision($db, $repository_id, $filter);
    } else {
        $matches = array();
        if (!preg_match('/([A-Za-z0-9]+)[\:\-]([A-Za-z0-9]+)/', $filter, $matches))
            exit_with_error('UnknownFilter', array('repositoryName' => $repository_name, 'filter' => $filter));

        $commits = fetch_commits_between($db, $repository_id, $matches[1], $matches[2]);
    }

    exit_with_success(array('commits' => $single_commit ? format_commit($single_commit) : $commits));
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

function fetch_commits_between($db, $repository_id, $first, $second, $keyword = NULL) {
    $statements = 'SELECT commit_id as "id",
        commit_revision as "revision",
        commit_parent as "parent",
        commit_time as "time",
        commit_author_name as "authorName",
        commit_author_email as "authorEmail",
        commit_message as "message"
        FROM commits WHERE commit_repository = $1 AND commit_reported = true';
    $values = array($repository_id);

    if ($first && $second) {
        $fitrt_commit = commit_from_revision($db, $repository_id, $first);
        $second_commit = commit_from_revision($db, $repository_id, $second);
        $first = $fitrt_commit['commit_time'];
        $second = $second_commit['commit_time'];
        $in_order = $first < $second;
        array_push($values, $in_order ? $first : $second);
        $statements .= ' AND commit_time >= $' . count($values);
        array_push($values, $in_order ? $second : $first);
        $statements .= ' AND commit_time <= $' . count($values);
    }

    if ($keyword) {
        array_push($values, '%' . str_replace(array('\\', '_', '@'), array('\\\\', '\\_', '\\%'), $keyword) . '%');
        $index = '$' . count($values);
        $statements .= " AND (commit_author_name LIKE $index OR commit_author_email LIKE $index";
        array_push($values, ltrim($keyword, 'r'));
        $statements .= ' OR commit_revision = $' . count($values) . ')';
    }

    $commits = $db->query_and_fetch_all($statements . ' ORDER BY commit_time', $values);
    if (!is_array($commits))
        exit_with_error('FailedToFetchCommits', array('repository' => $repository_id, 'first' => $first, 'second' => $second));
    return $commits;
}

function format_commit($commit_row) {
    return array(array(
        'id' => $commit_row['commit_id'],
        'revision' => $commit_row['commit_revision'],
        'parent' => $commit_row['commit_parent'],
        'time' => $commit_row['commit_time'],
        'authorName' => $commit_row['commit_author_name'],
        'authorEmail' => $commit_row['commit_author_email'],
        'message' => $commit_row['commit_message']
    ));
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
