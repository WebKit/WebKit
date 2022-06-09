<?php get_header(); ?>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article <?php echo post_class(); ?> id="post-<?php the_ID(); ?>">
            <h1><?php before_the_title(); ?><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>
            <div class="byline">
                <p class="date"><?php the_time('M j, Y')?></p>
                <p class="author">by <span><?php the_author() ?></span></p>
                <?php if ( '' !== ( $twitter_handle = get_the_author_meta('twitter') ) ): ?>
                <p class="twitter"><a href="https://twitter.com/<?php echo get_the_author_meta('twitter'); ?>" target="_blank">@<?php echo esc_html($twitter_handle); ?></a></p>
                <?php endif; ?>
            </div>

            <div class="bodycopy">
                <?php the_content('<p class="serif">Read the rest of this entry &gt;&gt;</p>'); ?>

                <?php link_pages('<p><strong>Pages:</strong> ', '</p>', 'number'); ?>
            </div>
        </article>

        <aside class="nextrouter" aria-label="Next/Previous posts">
            <div class="bodycopy">
            <?php //previous_post_link('%link', 'Older Post <span>%title</span>'); ?>
            <?php next_post_link('%link', '<div class="nextrouter-copy"><span class="label">Next</span><span class="title">%title</span><span class="link">Learn more</span></div>'); ?>
            </div>
        </aside>
        <aside class="nextrouter previous" aria-label="Next/Previous posts">
            <div class="bodycopy">
            <?php previous_post_link('%link', '<div class="nextrouter-copy"><span class="label">Previously</span><span class="title">%title</span><span class="link">Learn more</span></div>'); ?>
            </div>
        </aside>

    <?php //comments_template(); // No comments ?>

    <?php endwhile; else:
        include('444.php');
    endif; ?>

<?php get_footer(); ?>