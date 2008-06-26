<?php get_header(); ?>

        <?php if (have_posts()) : ?>
                
                <?php while (have_posts()) : the_post(); ?>

                        <div class="post" id="post-<?php the_ID(); ?>">
                                <h2><a href="<?php the_permalink() ?>" rel="bookmark" title="Permanent Link to <?php the_title(); ?>"><?php the_title(); ?></a></h2>
                                <small>Posted by <strong><?php the_author() ?></strong> on <?php the_time('l, F jS, Y') ?> at <?php the_time('g:i a') ?></small>

                                <div class="entry">
                                        <?php the_content('Read the rest of this entry &raquo;'); ?>
                                </div>

                                <p class="postmetadata"><?php edit_post_link('Edit','','<strong>|</strong>'); ?>  <?php comments_popup_link('No Comments...', '1 Comment...', '% Comments...'); ?></p>
                        </div>

                <?php endwhile; ?>

                <div class="navigation">
                        <div class="alignleft"><?php next_posts_link('&laquo; Previous Entries') ?></div>
                        <div class="alignright"><?php previous_posts_link('Next Entries &raquo;') ?></div>
                </div>

        <?php else : ?>

                <h2 class="center">Not Found</h2>
                <p class="center">Sorry, but you are looking for something that isn't here.</p>
                <?php include (TEMPLATEPATH . "/searchform.php"); ?>

        <?php endif; ?>

<?php get_footer(); ?>
