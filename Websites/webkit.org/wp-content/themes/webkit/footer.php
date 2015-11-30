    </main><!-- #content -->
        
    <footer class="page-width">
    <hr>
    <nav id="footer-nav" aria-label="Footer menu"><?php wp_nav_menu( array('theme_location'  => 'footer-nav') ); ?></nav>
    </footer>

<?php if ( is_front_page() ): ?></div> <!-- .page-layer --><?php endif; ?>

<?php wp_footer() ?>
</body>
</html>