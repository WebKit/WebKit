<?php
/*
Plugin Name: Web Inspector Pages
Description: Adds Web Inspector reference page support to WordPress
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

register_activation_hook(__FILE__, function () {
    add_role('web_inspector_page_editor', __('Web Inspector Editor'), array(
        'read_web_inspector_page' => true,
        'read_web_inspector_pages' => true,
        'read_private_web_inspector_pages' => true,
        'edit_web_inspector_page' => true,
        'edit_web_inspector_pages' => true,
        'edit_others_web_inspector_pages' => true,
        'edit_published_web_inspector_pages' => true,
        'delete_web_inspector_page' => true,
        'delete_web_inspector_pages' => true,
        'publish_web_inspector_pages' => true,

        // Standard Editor Capabilities
        'delete_others_pages' => true,
        'delete_others_posts' => true,
        'delete_pages' => true,
        'delete_posts' => true,
        'delete_private_pages' => true,
        'delete_private_posts' => true,
        'delete_published_pages' => true,
        'delete_published_posts' => true,
        'edit_others_pages' => true,
        'edit_others_posts' => true,
        'edit_pages' => true,
        'edit_posts' => true,
        'edit_private_pages' => true,
        'edit_private_posts' => true,
        'edit_published_pages' => true,
        'edit_published_posts' => true,
        'manage_categories' => true,
        'manage_links' => true,
        'moderate_comments' => true,
        'publish_pages' => true,
        'publish_posts' => true,
        'read' => true,
        'read_private_pages' => true,
        'read_private_posts' => true,
        'unfiltered_html' => true,
        'upload_files' => true
    ));

    $role = get_role('administrator');
    $role->add_cap('read_web_inspector_page');
    $role->add_cap('read_web_inspector_pages');
    $role->add_cap('read_private_web_inspector_pages');
    $role->add_cap('edit_web_inspector_page');
    $role->add_cap('edit_web_inspector_pages');
    $role->add_cap('edit_others_web_inspector_pages');
    $role->add_cap('edit_private_web_inspector_pages');
    $role->add_cap('edit_published_web_inspector_pages');
    $role->add_cap('delete_web_inspector_page');
    $role->add_cap('delete_web_inspector_pages');
    $role->add_cap('delete_others_web_inspector_pages');
    $role->add_cap('delete_private_web_inspector_pages');
    $role->add_cap('delete_published_web_inspector_pages');
    $role->add_cap('publish_web_inspector_pages');
});

add_action('init', function () {    
    register_post_type('web_inspector_page', array(
        'label'                 => 'Web Inspector Page',
        'description'           => 'Reference documentation for Web Inspector.',
        'menu_icon'             => 'dashicons-media-document',
        'labels'                => array(
            'name'                  => 'Web Inspector Reference',
            'singular_name'         => 'Web Inspector Page',
            'menu_name'             => 'Web Inspector Pages',
            'name_admin_bar'        => 'Web Inspector Page',
            'archives'              => 'Web Inspector Page Archives',
            'attributes'            => 'Web Inspector Page Attributes',
            'parent_item_colon'     => 'Web Inspector Page:',
            'all_items'             => 'All Pages',
            'add_new_item'          => 'Add New Web Inspector Page',
            'add_new'               => 'Add New',
            'new_item'              => 'New Page',
            'edit_item'             => 'Edit Page',
            'update_item'           => 'Update Page',
            'view_item'             => 'View Page',
            'view_items'            => 'View Pages',
            'search_items'          => 'Search Page',
            'not_found'             => 'Not found',
            'not_found_in_trash'    => 'Not found in Trash',
            'featured_image'        => 'Featured Image',
            'set_featured_image'    => 'Set featured image',
            'remove_featured_image' => 'Remove featured image',
            'use_featured_image'    => 'Use as featured image',
            'insert_into_item'      => 'Insert intopage',
            'uploaded_to_this_item' => 'Uploaded to this page',
            'items_list'            => 'Pages list',
            'items_list_navigation' => 'Pages list navigation',
            'filter_items_list'     => 'Filter pages list',
        ),
        'supports'              => array('title', 'editor', 'author', 'excerpt', 'thumbnail', 'revisions', 'custom-fields', 'page-attributes'),
        'taxonomies'            => array('web_inspector_topics'),
        'hierarchical'          => true,
        'public'                => true,
        'show_ui'               => true,
        'show_in_menu'          => true,
        'menu_position'         => 7,
        'show_in_admin_bar'     => true,
        'show_in_nav_menus'     => true,
        'can_export'            => true,
        'has_archive'           => true,
        'exclude_from_search'   => false,
        'publicly_queryable'    => true,
        'rewrite'               => array(
            'slug'                  => 'web-inspector',
            'with_front'            => false,
            'pages'                 => true,
            'feeds'                 => true,
        ),
        'capabilities'          => array(
            'edit_post'             => 'edit_web_inspector_page',
            'read_post'             => 'read_web_inspector_page',
            'delete_post'           => 'delete_web_inspector_page',
            'edit_posts'            => 'edit_web_inspector_pages',
            'edit_others_posts'     => 'edit_others_web_inspector_pages',
            'publish_posts'         => 'publish_web_inspector_pages',
            'read_private_posts'    => 'read_private_web_inspector_pages',
        ),
        'capability_type'       => array('web_inspector_page','web_inspector_pages'),
        'map_meta_cap'          => true,
        'show_in_rest'          => true
    ));

    register_taxonomy('web_inspector_topics', array('web_inspector_page'), array(
        'labels'                     => array(
            'name'                       => _x('Web Inspector Topics', 'Taxonomy General Name'),
            'singular_name'              => _x('Web Inspector Topic', 'Taxonomy Singular Name'),
            'menu_name'                  => __('Web Inspector Topics'),
            'all_items'                  => __('All Web Inspector Topics'),
            'parent_item'                => __('Parent Web Inspector Topic'),
            'parent_item_colon'          => __('Parent Web Inspector Topic:'),
            'new_item_name'              => __('New Web Inspector Topic'),
            'add_new_item'               => __('Add New Topic'),
            'edit_item'                  => __('Edit Topic'),
            'update_item'                => __('Update Topic'),
            'view_item'                  => __('View Topic'),
            'separate_items_with_commas' => __('Separate topics with commas'),
            'add_or_remove_items'        => __('Add or remove topics'),
            'choose_from_most_used'      => __('Choose from the most used'),
            'popular_items'              => __('Popular Topics'),
            'search_items'               => __('Search Topics'),
            'not_found'                  => __('Not Found'),
            'no_terms'                   => __('No topics'),
            'items_list'                 => __('Topics list'),
            'items_list_navigation'      => __('Topics list navigation'),
        ),
        'hierarchical'               => false,
        'public'                     => true,
        'show_ui'                    => true,
        'show_admin_column'          => true,
        'show_in_nav_menus'          => true,
        'show_tagcloud'              => true
    ));

    add_post_type_support('web_inspector_page', 'wpcom-markdown');
});