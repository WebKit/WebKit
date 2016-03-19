<?php

class CommitLogFetcher {

    function __construct($db) {
        $this->db = $db;
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
            $commit = $this->format_commit($commit_row, $commit_row);
            $commit['repository'] = $commit_row['commit_repository'];
            array_push($commits, $commit);
            array_push($associated_task[Database::is_true($commit_row['taskcommit_is_fix']) ? 'fixes' : 'causes'], $commit_row['commit_id']);
        }
        return $commits;
    }

    function repository_id_from_name($name)
    {
        $repository_row = $this->db->select_first_row('repositories', 'repository', array('name' => $name));
        if (!$repository_row)
            return NULL;
        return $repository_row['repository_id'];
    }

    function fetch_between($repository_id, $first, $second, $keyword = NULL) {
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
            $first_commit = $this->commit_for_revision($repository_id, $first);
            $second_commit = $this->commit_for_revision($repository_id, $second);
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

        $commits = $this->db->query_and_fetch_all($statements . ' ORDER BY commit_time, commit_order', $values);
        if (!is_array($commits))
            return NULL;

        foreach ($commits as &$commit)
            $commit['time'] = Database::to_js_time($commit['time']);

        return $commits;
    }

    function fetch_oldest($repository_id) {
        return $this->format_single_commit($this->db->select_first_row('commits', 'commit', array('repository' => $repository_id), 'time'));
    }

    function fetch_latest($repository_id) {
        return $this->format_single_commit($this->db->select_first_row('commits', 'commit', array('repository' => $repository_id), 'time'));
    }

    function fetch_last_reported($repository_id) {
        return $this->format_single_commit($this->db->select_last_row('commits', 'commit', array('repository' => $repository_id, 'reported' => true), 'time'));
    }

    function fetch_revision($repository_id, $revision) {
        return $this->format_single_commit($this->db->commit_for_revision($repository_id, $revision));
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
        $committer = $this->db->select_first_row('committers', 'committer', array('id' => $commit_row['commit_committer']));
        return array($this->format_commit($commit_row, $committer));
    }

    private function format_commit($commit_row, $committer_row) {
        return array(
            'id' => $commit_row['commit_id'],
            'revision' => $commit_row['commit_revision'],
            'parent' => $commit_row['commit_parent'],
            'time' => Database::to_js_time($commit_row['commit_time']),
            'order' => $commit_row['commit_order'],
            'authorName' => $committer_row ? $committer_row['committer_name'] : null,
            'authorEmail' => $committer_row ? $committer_row['committer_account'] : null,
            'message' => $commit_row['commit_message']
        );
    }
}

?>