<?php

class CommitUpdater {
    private $db;
    private $top_level_repository_id_by_name;
    private $owned_repository_by_name_and_owner_id;
    private $commits_by_repository_id;
    private $authors_by_repository_id;

    function __construct($db)
    {
        $this->db = $db;
        $this->top_level_repository_id_by_name = array();
        $this->owned_repository_by_name_and_owner_id = array();
        $this->commits_by_repository_id = array();
        $this->authors_by_repository_id = array();
    }

    function report_commits(&$commit_info_list, $should_insert)
    {
        $update_list = &$this->construct_update_list($commit_info_list, $should_insert);
        $this->db->begin_transaction();

        foreach ($update_list as &$update)
            $this->update_commit($update, NULL, NULL, $should_insert);

        $this->db->commit_transaction();
    }

    private function update_commit(&$update, $owner_commit_id, $owner_repository_id, $should_insert)
    {
        $commit_data = &$this->resolve_fields_from_database($update, $owner_repository_id, $should_insert);

        $commit_select_query = array('repository' => $commit_data['repository'], 'revision' => $commit_data['revision']);
        if ($should_insert) {
            $commit_id = $this->db->update_or_insert_row('commits', 'commit', $commit_select_query, $commit_data);
            if (!$commit_id)
                $this->exit_with_error('FailedToInsertCommit', array('commit' => $commit_data));

            if ($owner_commit_id)
                $this->db->select_or_insert_row('commit_ownerships', 'commit', array('owner' => $owner_commit_id, 'owned' => $commit_id), NULL, '*');
        } else {
            $commit_id = $this->db->update_row('commits', 'commit', $commit_select_query, $commit_data);
            if (!$commit_id)
                $this->exit_with_error('FailedToUpdateCommit', array('commit' => $commit_select_query, 'commitUpdate' => $commit_data));

            if ($owner_commit_id)
                $this->exit_with_error('AttemptToUpdateOwnedCommits', array('commit' => $commit_select_query));
        }

        if (!array_key_exists('owned_commits', $update))
            return;

        foreach ($update['owned_commits'] as &$owned_commit_update)
            $this->update_commit($owned_commit_update, $commit_id, $commit_data['repository'], $should_insert);
    }

    private function &resolve_fields_from_database(&$update, $owner_repository_id, $should_insert)
    {
        $commit_data = &$update['commit'];

        $repository_id = $this->resolve_repository($update['repository'], $owner_repository_id, $should_insert);
        $commit_data['repository'] = $repository_id;

        if (array_key_exists('previous_commit', $update))
            $commit_data['previous_commit'] = $this->resolve_previous_commit($repository_id, $update['previous_commit']);

        if (array_key_exists('author', $update))
            $commit_data['committer'] = $this->resolve_committer($repository_id, $update['author']);

        return $commit_data;
    }

    private function &construct_update_list(&$commit_info_list, $should_insert)
    {
        $update_list = array();

        foreach ($commit_info_list as &$commit_info) {
            self::validate_commits($commit_info);
            $commit_data = &self::construct_commit_data($commit_info, NULL, $should_insert);
            $has_update = count($commit_data) > 1;

            $update = array('commit' => &$commit_data, 'repository' => &$commit_info['repository']);
            if (array_key_exists('previousCommit', $commit_info)) {
                $has_update = true;
                $update['previous_commit'] = &$commit_info['previousCommit'];
            }

            if (array_key_exists('author', $commit_info)) {
                $has_update = true;
                $update['author'] = &$commit_info['author'];
            }

            if (array_key_exists('ownedCommits', $commit_info) && !$should_insert)
                $this->exit_with_error('AttemptToUpdateOwnedCommits', array('commit' => $commit_info));

            if (!$should_insert && !$has_update)
                $this->exit_with_error('NothingToUpdate', array('commit' => $commit_data));

            if (!array_key_exists('ownedCommits', $commit_info)) {
                array_push($update_list, $update);
                continue;
            }

            $owned_commit_update_list = array();
            foreach($commit_info['ownedCommits'] as $owned_repository_name => &$owned_commit_info) {
                $owned_commit = &self::construct_commit_data($owned_commit_info, $owned_repository_name, $should_insert);
                $owned_commit_update = array('commit' => &$owned_commit, 'repository' => $owned_repository_name);

                if (array_key_exists('previousCommit', $owned_commit_info))
                    $owned_commit_update['previous_commit'] = $owned_commit_info['previousCommit'];

                if (array_key_exists('author', $owned_commit_info))
                    $owned_commit_update['author'] = &$owned_commit_info['author'];
                array_push($owned_commit_update_list, $owned_commit_update);
            }
            if (count($owned_commit_update_list))
                $update['owned_commits'] = $owned_commit_update_list;

            array_push($update_list, $update);
        }

        return $update_list;
    }

    private static function &construct_commit_data(&$commit_info, $owned_repository_name, $should_insert)
    {
        $commit_data = array();
        $commit_data['revision'] = $commit_info['revision'];

        if (array_key_exists('message', $commit_info))
            $commit_data['message'] = $commit_info['message'];

        if (array_key_exists('order', $commit_info))
            $commit_data['order'] = $commit_info['order'];

        if (array_key_exists('testability', $commit_info))
            $commit_data['testability'] = $commit_info['testability'];

        if (array_key_exists('time', $commit_info)) {
            if ($owned_repository_name)
                exit_with_error('OwnedCommitShouldNotContainTimestamp', array('commit' => $commit_info));
            $commit_data['time'] = $commit_info['time'];
        }

        if ($should_insert)
            $commit_data['reported'] = true;

        return $commit_data;
    }

    private static function validate_commits(&$commit_info)
    {
        if (!array_key_exists('repository', $commit_info))
            exit_with_error('MissingRepositoryName', array('commit' => $commit_info));
        if (!array_key_exists('revision', $commit_info))
            exit_with_error('MissingRevision', array('commit' => $commit_info));
        require_format('Revision', $commit_info['revision'], '/^[A-Za-z0-9 \.]+$/');
        if (array_key_exists('author', $commit_info) && !is_array($commit_info['author']))
            exit_with_error('InvalidAuthorFormat', array('commit' => $commit_info));
        if (array_key_exists('previousCommit', $commit_info))
            require_format('Revision', $commit_info['previousCommit'], '/^[A-Za-z0-9 \.]+$/');
    }

    private function resolve_repository($repository_name, $owner_repository_id, $should_insert)
    {
        if ($owner_repository_id)
            $repository_id_by_name = &array_ensure_item_has_array($this->owned_repository_by_name_and_owner_id, $owner_repository_id);
        else
            $repository_id_by_name = &$this->top_level_repository_id_by_name;

        if (array_key_exists($repository_name, $repository_id_by_name))
            return $repository_id_by_name[$repository_name];

        if ($should_insert) {
            $repository_id = $this->db->select_or_insert_row('repositories', 'repository', array('name' => $repository_name, 'owner' => $owner_repository_id));
            if (!$repository_id)
                $this->exit_with_error('FailedToInsertRepository', array('repository' => $repository_name, 'owner' => $owner_repository_id));
        } else {
            $repository = $this->db->select_first_row('repositories', 'repository', array('name' => $repository_name, 'owner' => $owner_repository_id));
            if (!$repository)
                $this->exit_with_error('InvalidRepository', array('repository' => $repository_name));
            $repository_id = $repository['repository_id'];
        }

        $repository_id_by_name[$repository_name] = $repository_id;
        return $repository_id;
    }

    private function resolve_previous_commit($repository_id, $revision)
    {
        $commit_id_by_revision = &array_ensure_item_has_array($this->commits_by_repository_id, $revision);
        if (array_key_exists($revision, $commit_id_by_revision))
            return $commit_id_by_revision[$revision];

        $previous_commit = $this->db->select_first_row('commits', 'commit', array('repository' => $repository_id, 'revision' => $revision));
        if (!$previous_commit)
            $this->exit_with_error('FailedToFindPreviousCommit', array('revision' => $revision));

        $commit_id_by_revision[$revision] = $previous_commit['commit_id'];
        return $previous_commit['commit_id'];
    }

    private function resolve_committer($repository_id, $author)
    {
        $committer_id_by_account = &array_ensure_item_has_array($this->authors_by_repository_id, $repository_id);
        $account = array_get($author, 'account');
        if (array_key_exists($account, $committer_id_by_account))
            return $committer_id_by_account[$account];

        $committer_data = array('repository' => $repository_id, 'account' => $account);
        $name = array_get($author, 'name');
        if ($name)
            $committer_data['name'] = $name;
        $committer_id = $this->db->update_or_insert_row('committers', 'committer', array('repository' => $repository_id, 'account' => $account), $committer_data);

        if (!$committer_id)
            $this->exit_with_error('FailedToInsertCommitter', array('committer' => $committer_data));
        $committer_id_by_account[$account] = $committer_id;
        return $committer_id;
    }

    private function exit_with_error($message, $details)
    {
        $this->db->rollback_transaction();
        exit_with_error($message, $details);
    }
}