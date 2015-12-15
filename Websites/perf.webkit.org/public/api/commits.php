<?php

require_once('../include/json-header.php');

function main($paths) {
    if (count($paths) < 1 || count($paths) > 2)
        exit_with_error('InvalidRequest');

    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    if (!is_numeric($paths[0])) {
        $repository_name = $paths[0];
        $repository_row = $db->select_first_row('repositories', 'repository', array('name' => $repository_name));
        if (!$repository_row)
            exit_with_error('RepositoryNotFound', array('repositoryName' => $repository_name));
        $repository_id = $repository_row['repository_id'];
    } else
        $repository_id = intval($paths[0]);

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

    if ($single_commit) {
        $committer = $db->select_first_row('committers', 'committer', array('id' => $single_commit['commit_committer']));
        exit_with_success(array('commits' => array(format_commit($single_commit, $committer))));
    }

    exit_with_success(array('commits' => $commits));
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
        committer_name as "authorName",
        committer_account as "authorEmail",
        commit_message as "message"
        FROM commits LEFT OUTER JOIN committers ON commit_committer = committer_id
        WHERE commit_repository = $1 AND commit_reported = true';
    $values = array($repository_id);

    if ($first && $second) {
        $first_commit = commit_from_revision($db, $repository_id, $first);
        $second_commit = commit_from_revision($db, $repository_id, $second);
        $first = $first_commit['commit_time'];
        $second = $second_commit['commit_time'];
        $column_name = 'commit_time';
        if (!$first || !$second) {
            $first = $first_commit['commit_order'];
            $second = $second_commit['commit_order'];
            $column_name = 'commit_order';
        }

        $in_order = $first < $second;
        array_push($values, $in_order ? $first : $second);
        $statements .= ' AND ' . $column_name . ' >= $' . count($values);
        array_push($values, $in_order ? $second : $first);
        $statements .= ' AND ' . $column_name . ' <= $' . count($values);
    }

    if ($keyword) {
        array_push($values, '%' . str_replace(array('\\', '_', '%'), array('\\\\', '\\_', '\\%'), $keyword) . '%');
        $keyword_index = '$' . count($values);
        array_push($values, ltrim($keyword, 'r'));
        $revision_index = '$' . count($values);
        $statements .= "
            AND ((committer_name LIKE $keyword_index OR committer_account LIKE $keyword_index) OR commit_revision = $revision_index)";
    }

    $commits = $db->query_and_fetch_all($statements . ' ORDER BY commit_time, commit_order', $values);
    if (!is_array($commits))
        exit_with_error('FailedToFetchCommits', array('repository' => $repository_id, 'first' => $first, 'second' => $second));
    foreach ($commits as &$commit)
        $commit['time'] = Database::to_js_time($commit['time']);
    return $commits;
}

function format_commit($commit_row, $committer_row) {
    return array(
        'id' => $commit_row['commit_id'],
        'revision' => $commit_row['commit_revision'],
        'parent' => $commit_row['commit_parent'],
        'time' => Database::to_js_time($commit_row['commit_time']),
        'authorName' => $committer_row ? $committer_row['committer_name'] : null,
        'authorEmail' => $committer_row ? $committer_row['committer_account'] : null,
        'message' => $commit_row['commit_message']
    );
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
