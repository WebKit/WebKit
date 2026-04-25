<?php
/*
Plugin Name: Sync Nightly Builds
Description: Updates WebKit nightly build information
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

SyncWebKitNightlyBuilds::object();

class SyncWebKitNightlyBuilds {

    private static $object = null;

    const MAX_SEARCH_RESULTS = 50;
    const WEBKIT_NIGHTLY_ARCHIVE_URL = 'https://nightly.webkit.org/builds/trunk/%s/all';

    private $archives = array(
        'mac' => 'builds.csv',
        'src' => 'source.csv'
    );

    private $uploads_dir = '';

    public static function object() {
        if (self::$object === null)
            self::$object = new self();
        return self::$object;
    }

    private function __construct() {
        $upload_dir_info = wp_upload_dir();
        $this->uploads_dir = trailingslashit($upload_dir_info['basedir']);

        add_action('update_webkit_nightly_builds', array($this, 'sync'));
        add_action('wp_ajax_nopriv_search_nightly_builds', array($this, 'search'));
        add_action('wp_ajax_search_nightly_builds', array($this, 'search'));

        register_activation_hook(__FILE__, function () {
            if (!wp_next_scheduled('update_webkit_nightly_builds'))
                wp_schedule_event(current_time('timestamp'), 'hourly', 'update_webkit_nightly_builds');

            do_action('update_webkit_nightly_builds');
        });
    }

    public function sync() {
        foreach ($this->archives as $endpoint => $filename) {
            // Download a copy of the archives
            $url = sprintf(self::WEBKIT_NIGHTLY_ARCHIVE_URL, $endpoint);

            if (!copy($url, $this->uploads_dir. $filename))  {
                error_log("Couldn't download the $filename archive index. ({$errors['type']}) {$errors['message']}");
            }
        }
    }

    public function latest($archive = 'builds') {
        $records = $this->records($archive);
        return (array)$records[0];
    }

    public function records($archive, $entries = 25) {
        if (!in_array("$archive.csv", $this->archives)) return array();

        $filename = "$archive.csv";
        if (!file_exists($this->uploads_dir . '/' . $filename)) {
            do_action('update_webkit_nightly_builds');
            if (!file_exists($this->uploads_dir . '/' . $filename))
                return array();
        }

        $records = array();
        $resource = fopen($this->uploads_dir . '/' . $filename, 'r');

        for ($i = 0; $i < $entries; $i++) {
            $line = fgets($resource, 128);
            $line = trim($line);
            if ( empty($line) ) break;
            list($build, $timestamp, $download) = explode(',', $line);
            $records[$i] = array(
                $build, 
                date("F j, Y g:i A", $timestamp) . " GMT", 
                str_replace("http://builds.nightly.webkit.org/", "https://builds-nightly.webkit.org/", $download)
            );
        }
        fclose($resource);
        return (array)$records;
    }

    public function search() {
        // http://webkit.dev/wp/wp-admin/admin-ajax.php?action=search_nightly_builds&search=194142
        // http://webkit.dev/wp/wp-admin/admin-ajax.php?action=search_nightly_builds&search=1941
        // http://webkit.dev/wp/wp-admin/admin-ajax.php?action=search_nightly_builds&search=193760-194142
        // http://webkit.dev/wp/wp-admin/admin-ajax.php?action=search_nightly_builds&search=193760

        $maxresults = self::MAX_SEARCH_RESULTS;

        // Clean up search query
        $search = trim($_GET['query']);
        $search = trim($search, '-');
        $search = str_replace('r', '', $search);

        if (empty($search)) {
            echo json_encode('');
            wp_die();
        }

        $records = $this->records('builds', 100000);

        $results = array();
        $matches = array();
        $builds = array();
        foreach ($records as $record)
            $builds[] = $record[0];


        $methods = array('prefixmatch', 'rangematch', 'nearmatch');

        $matched = 0;
        // Manual exact match for performance
        $exactmatch = array_search($search, $builds);
        if ($exactmatch !== false) {
            $matches['exactmatch']["r" . $records[$exactmatch][0]] = $records[$exactmatch];
            $matched = 1;
        } else {
            foreach ($builds as $index => $build) {
                foreach ($methods as $method) {
                    if ($matched >= $maxresults) break;
                    if ($this->$method($search, $builds, $build, $index)) {
                        $matches[$method]["r$build"] = $records[$index];
                        $matched++;
                    }
                }
            }
        }

        // Compile results
        foreach ($matches as $group)
            $results = array_merge($results, $group);

        echo json_encode(array_values($results));

        wp_die();
    }

    public function prefixmatch($search, &$builds, $build) {
        return substr($build, 0, strlen($search)) == $search;
    }

    public function rangematch($search, &$builds, $build) {
        if ( strpos($search, '-') === false ) return false;

        $ranges = explode('-', $search);
        $ranges = array_map('intval', $ranges);
        sort($ranges);
        list($start, $end) = $ranges;
        $build = intval($build);

        if ( $end < 1 ) $end = PHP_INT_MAX;

        return $build >= $start && $build <= $end;
    }

    public function nearmatch($search, &$builds, $build, $index) {
        $target = intval($search);
        if ($target < 1) return false;

        $newer = isset($builds[ $index - 1 ]) ? $builds[ $index - 1 ] : PHP_INT_MAX;
        $older = isset($builds[ $index + 1 ]) ? $builds[ $index + 1 ] : 0;

        return $target > $older && $target < $newer;
    }

} // end class SyncWebKitNightlyBuilds