<?php
/*
Plugin Name: Social Meta
Description: Adds schema.org, Twitter Card, Open Graph meta for posts and pages
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

add_action('wp_head', function() {
    
    $title = get_the_title();
    $description = get_the_excerpt();
    $type = 'article';
    
    $categories = array();
    $tags = array();
    $image_url = '';
    $twitter_handle = '';
    
    if (is_front_page() || is_home()) {
        $title = get_bloginfo('name');
        $description = get_bloginfo('description');
        $type = 'website';      
    }
    
    if (is_single()) {
        $image_src = wp_get_attachment_image_src( get_post_thumbnail_id($post_id), 'large' );
        $image_url = $image_src[0];
    
        $categories = wp_get_object_terms(get_the_ID(), 'category', array('fields' => 'names'));
        $tags = wp_get_object_terms(get_the_ID(), 'post_tag', array('fields' => 'names'));

        // Post author data is not available in wp_head filter context
        $post = get_post();
        $twitter_handle = get_the_author_meta('twitter', $post->post_author);
    }
    
?>

    <!-- Schema.org markup -->
    <meta itemprop="name" content="<?php echo esc_attr($title); ?>">
<?php if ($description): ?>
    <meta itemprop="description" content="<?php echo esc_attr($description); ?>">
<?php endif; ?>
<?php if ($image_url): ?>
    <meta itemprop="image" content="<?php echo esc_url($image_url); ?>">
<?php endif; ?>

    <!-- Twitter Card data -->
    <meta name="twitter:card" content="summary_large_image">
    <meta name="twitter:site" content="@webkit">
    <meta name="twitter:title" content="<?php echo esc_attr($title); ?>">
<?php if ($description): ?>
    <meta name="twitter:description" content="<?php echo esc_attr($description); ?>">
<?php endif; ?>
<?php if ($twitter_handle): ?>
    <meta name="twitter:creator" content="@<?php echo esc_attr($twitter_handle); ?>">
<?php endif; ?>
<?php if ($image_url): // Twitter summary card with large image must be at least 280x150px ?>
    <meta name="twitter:image:src" content="<?php echo esc_url($image_url); ?>">
<?php endif; ?>

    <!-- Open Graph data -->
    <meta property="og:title" content="<?php echo esc_attr($title); ?>" />
    <meta property="og:type" content="<?php echo esc_attr($type); ?>" />
    <meta property="og:url" content="<?php the_permalink(); ?>" />
<?php if ($image_url): ?>
    <meta itemprop="og:image" content="<?php echo esc_url($image_url); ?>">
<?php endif; ?>
<?php if ($description): ?>
    <meta property="og:description" content="<?php echo esc_attr($description); ?>" />
<?php endif; ?>
    <meta property="og:site_name" content="<?php bloginfo('title'); ?>" />
    <meta property="article:published_time" content="<?php the_time('c'); ?>" />
    <meta property="article:modified_time" content="<?php the_modified_date('c'); ?>" />
<?php
    if (!empty($categories)):
        $section = array_shift($categories);      // The first category is used as the section
        $tags = array_merge($categories, $tags);  // The rest are prepended to the tag list
?>
    <meta property="article:section" content="<?php echo esc_attr($section); ?>" />
<?php
    endif;

    if (!empty($tags)):
        foreach($tags as $tag):
?>
    <meta property="article:tag" content="<?php echo esc_attr($tag); ?>" />
<?php 
        endforeach; 
    endif;
?>

<?php
});