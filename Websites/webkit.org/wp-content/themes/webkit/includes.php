<?php
/**
 * Template Name: Included Page
 **/
?>
<?php get_header(); ?>

    <?php if (have_posts()) : while (have_posts()) : the_post();

        $path = dirname(ABSPATH);
        $file = get_post_meta(get_the_ID(), 'include-markdown', true);

        $cachekey = "cached_include_" . $file;
        if ( is_super_cache_enabled() || false === ( $content = get_transient($cachekey) ) ) {
            ob_start();
            include $path . '/' . $file;
            $content = ob_get_clean();

            // Handle sub-section anchors
            $content = preg_replace('/\[]\(\#([^\)]+)\)\s+?/', '<a href="#$1" name="$1"></a>', $content);

            // Transform Markdown
            $Markdown = WPCom_Markdown::get_instance();
            $content = wp_unslash( $Markdown->transform($content) );

            // Index table of contents
            $content = table_of_contents_index($content, get_the_ID());

            set_transient($cachekey, $content, DAY_IN_SECONDS);
        }

    ?>

        <article class="page<?php if ( has_table_of_contents() ) echo ' with-toc';?>" id="post-<?php the_ID(); ?>">

            <h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <div class="bodycopy">
            <?php table_of_contents(); ?>
            <?php
                echo apply_filters('the_content', $content);
            ?>
            </div>
        </article>

    <?php //comments_template(); ?>

    <?php endwhile; else:
        include('444.php');
    endif; ?>

<?php get_footer(); ?>
