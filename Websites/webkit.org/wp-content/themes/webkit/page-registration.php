<?php
if (class_exists('GitHubOAuthPlugin'))
    GitHubOAuthPlugin::request_auth();

if (!class_exists('WebKit_Meeting_Registration'))
    return false;

WebKit_Meeting_Registration::process();

get_header();
?>
    <style>
        form {
            display: flex;
            flex-wrap: wrap;
            gap: 3rem;
        }

        #registration {
            margin: 6rem auto;
        }

        article h1, article h1 a {
            font-size: 3.5rem;
        }
    </style>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>
        <article class="page" id="registration">
            <h1><?php the_title(); ?></h1>
            <div class="bodycopy">
                <?php
                $registration = WebKit_Meeting_Registration::full_registration();
                if (!empty($registration)):
                ?>
                    <h3>You are registered!</h3>
                    <p>Please take a moment to verify your email address is updated in the <a href="https://github.com/WebKit/WebKit/blob/main/metadata/contributors.json"><code>contributors.json</code></a> file.</p>
                    <table>
                        <tr><td>Name</td><td><?php echo esc_html($registration->contributor_name); ?></td></tr>
                        <tr><td>Email</td><td><?php echo esc_html($registration->contributor_email); ?></td></tr>

                        <?php if(!empty($registration->contributor_affiliation)): ?>
                            <tr><td>Affiliation/Company</td><td><?php echo esc_html($registration->contributor_affiliation); ?></td></tr>
                        <?php endif; ?>

                        <?php if(!empty($registration->contributor_slack)): ?>
                            <tr><td>Slack Name</td><td><?php echo esc_html($registration->contributor_slack); ?></td></tr>
                        <?php endif; ?>

                        <?php if(!empty($registration->contributor_interests)): ?>
                            <tr><td>Interests</td><td><?php echo apply_filters('the_content', esc_html( $registration->contributor_interests)); ?></td></tr>
                        <?php endif; ?>
                    </table>

                    <nav class="navigation pagination">
                        <a href="/meeting" class="page-numbers next-post">Return<span>to the WebKit Contributors Meeting page</span></a>
                    </nav>
                <?php else: the_content(''); ?>
                <?php endif; ?>
            </div>

        </article>

    <?php endwhile; endif; ?>

<?php get_footer(); ?>