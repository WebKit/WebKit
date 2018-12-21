<?php

class CommitLogFetcher {

    function __construct($db) {
        $this->db = $db;
    }

    static function find_commit_id_by_revision($db, $repository_id, $revision)
    {
        $commit_rows = $db->query_and_fetch_all('SELECT commit_id FROM commits WHERE commit_repository = $1 AND commit_revision = $2', array($repository_id, $revision));
        if ($commit_rows)
            return $commit_rows[0]['commit_id'];

        if (!ctype_alnum($revision))
            return NULL;

        $commit_rows = $db->query_and_fetch_all('SELECT commit_id FROM commits WHERE commit_repository = $1 AND commit_revision LIKE $2 LIMIT 2', array($repository_id, Database::escape_for_like($revision) . '%'));
        if (!$commit_rows)
            return NULL;
        if (count($commit_rows) > 1)
            return -1; // There are more than one matches.
        return $commit_rows[0]['commit_id'];
    }

    function fetch_for_tasks($task_id_list, $task_by_id)
    {
        $commit_rows = $this->db->query_and_fetch_all('SELECT task_commits.*, commits.*, committers.*
            FROM task_commits, commits LEFT OUTER JOIN committers ON commit_committer = committer_id
            WHERE taskcommit_commit = commit_id AND taskcommit_task = ANY ($1)', array('{' . implode(', ', $task_id_list) . '}'));
        if (!is_array($commit_rows))
            return NULL;

        $commits = array();
        foreach ($commit_rows as &$commit_row) {
            $associated_task = &$task_by_id[$commit_row['taskcommit_task']];
            # FIXME: The last parameter should be determined based on commit_ownerships.
            $commit = $this->format_commit($commit_row, $commit_row, /* owns_commits */ FALSE);
            $commit['repository'] = $commit_row['commit_repository'];
            array_push($commits, $commit);
            array_push($associated_task[Database::is_true($commit_row['taskcommit_is_fix']) ? 'fixes' : 'causes'], $commit_row['commit_id']);
        }
        return $commits;
    }

    function repository_id_from_name($name)
    {
        $repository_row = $this->db->select_first_row('repositories', 'repository', array('name' => $name, 'owner' => NULL));
        if (!$repository_row)
            return NULL;
        return $repository_row['repository_id'];
    }

    function fetch_between($repository_id, $before_first_revision, $last_revision, $keyword = NULL)
    {
        $statements = 'SELECT commit_id as "id",
            commit_revision as "revision",
            commit_previous_commit as "previousCommit",
            commit_time as "time",
            committer_name as "authorName",
            committer_account as "authorEmail",
            commit_repository as "repository",
            commit_message as "message",
            commit_order as "order",
            commit_testability as "testability",
            EXISTS(SELECT * FROM commit_ownerships WHERE commit_owner = commit_id) as "ownsCommits"
            FROM commits LEFT OUTER JOIN committers ON commit_committer = committer_id
            WHERE commit_repository = $1 AND commit_reported = true';
        $values = array($repository_id);

        if ($before_first_revision && $last_revision) {
            $preceding_commit = $this->commit_for_revision($repository_id, $before_first_revision);
            $last_commit = $this->commit_for_revision($repository_id, $last_revision);

            $preceding = $preceding_commit['commit_time'];
            $last = $last_commit['commit_time'];
            if (!$preceding != !$last)
                exit_with_error('InconsistentCommits');
            if ($preceding)
                $column_name = 'commit_time';
            else {
                $preceding = $preceding_commit['commit_order'];
                $last = $last_commit['commit_order'];
                $column_name = 'commit_order';
                if (!$preceding || !$last)
                    return array();
            }
            if ($preceding >= $last)
                exit_with_error('InvalidCommitRange');

            array_push($values, $preceding);
            $statements .= ' AND ' . $column_name . ' > $' . count($values);
            array_push($values, $last);
            $statements .= ' AND ' . $column_name . ' <= $' . count($values);
        }

        if ($keyword) {
            array_push($values, '%' . Database::escape_for_like($keyword) . '%');
            $keyword_index = '$' . count($values);
            array_push($values, ltrim($keyword, 'r'));
            $revision_index = '$' . count($values);
            $statements .= "
                AND ((committer_name LIKE $keyword_index OR committer_account LIKE $keyword_index) OR commit_revision = $revision_index)";
        }

        $commits = $this->db->query_and_fetch_all($statements . ' ORDER BY commit_time, commit_order', $values);
        if (!is_array($commits))
            return NULL;

        foreach ($commits as &$commit) {
            $commit['time'] = Database::to_js_time($commit['time']);
            $commit['ownsCommits'] = Database::is_true($commit['ownsCommits']);
        }

        return $commits;
    }

    function fetch_owned_commits_for_revision($repository_id, $commit_revision) {
        return $this->db->query_and_fetch_all('SELECT owned.commit_repository as "repository",
            owned.commit_revision as "revision",
            owned.commit_time as "time",
            owned.commit_id as "id"
            FROM commits AS owned, commit_ownerships, commits AS owner
            WHERE owned.commit_id = commit_owned AND owner.commit_id = commit_owner AND owner.commit_revision = $1 AND owner.commit_repository = $2',
            array($commit_revision, $repository_id));
    }

    # FIXME: this is not DRY. Ideally, $db should provide the ability to search with criteria that specifies a range.
    function fetch_last_reported_between_orders($repository_id, $from, $to)
    {
        $statements = 'SELECT * FROM commits LEFT OUTER JOIN committers ON commit_committer = committer_id
            WHERE commit_repository = $1 AND commit_reported = true';
        $from = intval($from);
        $to = intval($to);
        $statements .= ' AND commit_order >= $2 AND commit_order <= $3 ORDER BY commit_order DESC LIMIT 1';

        $commits = $this->db->query_and_fetch_all($statements, array($repository_id, $from, $to));
        if (!is_array($commits))
            return NULL;

        foreach ($commits as &$commit)
            $commit = $this->format_single_commit($commit)[0];

        return $commits;
    }

    function fetch_oldest($repository_id) {
        return $this->format_single_commit($this->db->select_first_row('commits', 'commit', array('repository' => $repository_id), array('time', 'order')));
    }

    function fetch_latest($repository_id) {
        return $this->format_single_commit($this->db->select_last_row('commits', 'commit', array('repository' => $repository_id), array('time', 'order')));
    }

    function fetch_latest_for_platform($repository_id, $platform_id)
    {
        $query_result = $this->db->query_and_fetch_all("SELECT commits.* FROM test_runs, builds, build_commits, commits
            WHERE run_build = build_id AND NOT EXISTS (SELECT * FROM build_requests WHERE request_build = build_id)
                AND run_config IN (SELECT config_id FROM test_configurations
                    WHERE config_type = 'current' AND config_platform = $2 AND config_metric
                        IN (SELECT metric_id FROM test_metrics, tests WHERE metric_id = test_id and test_parent IS NULL))
                AND run_build = build_id AND commit_build = build_id AND build_commit = commit_id AND commit_repository = $1
            ORDER BY build_time DESC LIMIT 1;", array($repository_id, $platform_id));
        /* This query is approximation. It finds the commit of the last build instead of the last commit.
        We would ideally run the following query but it's much slower ~70s compared to 300ms.
            SELECT commits.*, commit_build, run_id
                FROM commits, build_commits, test_runs
                WHERE commit_repository = 9 AND commit_id = build_commit AND commit_build = run_build
                AND run_config IN (SELECT config_id FROM test_configurations WHERE config_platform = 38 AND config_type = 'current')
                AND NOT EXISTS (SELECT * FROM build_requests WHERE request_build = commit_build)
                ORDER BY commit_time DESC, commit_order DESC LIMIT 1; */
        if (!$query_result)
            return array();
        return $this->format_single_commit($query_result[0]);
    }

    function fetch_last_reported($repository_id) {
        return $this->format_single_commit($this->db->select_last_row('commits', 'commit', array('repository' => $repository_id, 'reported' => true), array('time', 'order')));
    }

    function fetch_revision($repository_id, $revision) {
        return $this->format_single_commit($this->commit_for_revision($repository_id, $revision));
    }

    private function commit_for_revision($repository_id, $revision) {
        $all_but_first = substr($revision, 1);
        if ($revision[0] == 'r' && ctype_digit($all_but_first))
            $revision = $all_but_first;
        $commit_info = array('repository' => $repository_id, 'revision' => $revision);
        $row = $this->db->select_last_row('commits', 'commit', $commit_info);
        if (!$row)
            exit_with_error('UnknownCommit', $commit_info);
        return $row;
    }

    private function format_single_commit($commit_row) {
        if (!$commit_row)
            return array();
        $committer = $this->db->select_first_row('committers', 'committer', array('id' => $commit_row['commit_committer']));
        $owns_commits = !!$this->db->select_first_row('commit_ownerships', 'commit', array('owner' => $commit_row['commit_id']));
        return array($this->format_commit($commit_row, $committer, $owns_commits));
    }

    private function format_commit($commit_row, $committer_row, $owns_commits) {
        return array(
            'id' => $commit_row['commit_id'],
            'revision' => $commit_row['commit_revision'],
            'repository' => $commit_row['commit_repository'],
            'previousCommit' => $commit_row['commit_previous_commit'],
            'time' => Database::to_js_time($commit_row['commit_time']),
            'order' => $commit_row['commit_order'],
            'authorName' => $committer_row ? $committer_row['committer_name'] : null,
            'authorEmail' => $committer_row ? $committer_row['committer_account'] : null,
            'message' => $commit_row['commit_message'],
            'testability' => $commit_row['commit_testability'],
            'ownsCommits' => $owns_commits
        );
    }
}

?>
