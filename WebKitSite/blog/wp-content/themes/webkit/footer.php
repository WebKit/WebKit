</div>

<?php get_sidebar(); ?>

<div id="footer">
	<?php bloginfo('name'); ?> site is powered by 
	<a href="http://wordpress.org">WordPress</a>
	<br /><a href="feed:<?php bloginfo('rss2_url'); ?>">Entries (RSS)</a>
	and <a href="feed:<?php bloginfo('comments_rss2_url'); ?>">Comments (RSS)</a>.
	<br> <?php  wp_register("", " <strong>|</strong> "); wp_loginout(); ?>
	<!-- <?php echo $wpdb->num_queries; ?> queries. <?php timer_stop(1); ?> seconds. -->
</div>

<?php wp_footer(); ?>

</body>
</html>