<style>
.webkit-survey-form ul {
    list-style: none;
}

.webkit-survey-form ul li {
    display: block;
    margin: 0 3rem;
    position: relative;
    vertical-align: top;
    box-sizing: border-box;
    overflow: hidden;
    min-height: 62px;
    perspective: 500px;
}

.webkit-survey-form input[type=text] {
    display: inline-block;
    vertical-align: baseline;
    margin-left: 1rem;
    margin-bottom: 0;
    backface-visibility: hidden;
    transform: rotateX( 90deg );
    transition: transform 500ms ease;
    visibility: hidden;
}

.webkit-survey-form input[type=radio] {
    margin-top: 0;
    vertical-align: middle;
    transition: opacity 500ms ease;
}

.webkit-survey-form input[type=radio]:checked ~ input[type=text] {
    transform: rotateX( 0deg );
    visibility: visible;
}

.webkit-survey-form input[type=radio]:checked,
.webkit-survey-form input[type=radio]:checked + span {
    opacity: 1;
    color: var(--text-color);
}

.webkit-survey-form input[type=radio] + span {
    color: var(--text-color-medium);
    transition: color 500ms ease;
}

.webkit-survey-form input[type=submit],
.webkit-survey-form a.button {
    padding: 1rem 3rem;
}

.webkit-survey-form label {
    box-sizing: border-box;
    display: inline-block;
    border-radius: 0.3rem;
    padding: 1rem 3rem 1rem 5rem;
    transition: background-color 500ms ease;
    cursor: pointer;
    text-indent: -2.4rem;
}

.webkit-survey-form ul label:hover {
    background: var(--tile-background-color);
}

.webkit-survey-form .button-align-right {
    text-align: right;
}

.webkit-survey-form .preview-button {
    background-color: var(--button-background-color);
    border-radius: 4px;
    color: var(--text-color);
    cursor: pointer;
    font-size: 1.5rem;
    font-weight: 500;
    text-align: center;
    border: 0;
    padding: 1rem 3rem;
}
.webkit-survey-form .preview-button:hover {
    background-color: var(--text-color-coolgray);
    color: white;
}
</style>

<form class="webkit-survey-form" method="POST">
<?php
wp_nonce_field("webkit_survey-" . get_the_ID(), "_nonce", $true);
foreach ($Survey->survey as $id => $Entry):
?>
    <h3><?php echo $Entry->question; ?></h3>
    
    <ul>
    <?php foreach ($Entry->responses as $value => $response): ?>
        <li><label><input type="radio" name="questions[<?php echo esc_attr($id); ?>]" value="<?php echo esc_attr($value); ?>" required> <span><?php echo esc_html($response); ?></span>
        <?php if ($response == "Other:"): ?>
            <input type="text" name="other[<?php echo esc_attr($id); ?>]" maxlength="144">
        <?php endif; ?>
        </label></li>
    <?php endforeach; ?>
    </ul>
<?php endforeach; ?>
<p class="button-align-right">
    <?php if ($SurveyResults->results != 'visible'): ?>
    <input type="button" name="preview" value="View Results" class="preview-button">
    <?php endif; ?>
    <input type="submit" name="Submit" value="Submit" class="submit-button">
</p>
</form>
<?php if ($SurveyResults->results != 'visible'): ?>
<script>
    let surveys = Array.from(document.getElementsByClassName('webkit-survey'));
    surveys.forEach(survey => {
        let form = survey.getElementsByClassName('webkit-survey-form')[0],
            results = survey.getElementsByClassName('webkit-survey-results')[0],
            previewButton = document.getElementsByClassName('preview-button')[0];
        previewButton.addEventListener('click', () => {
            results.classList.add('visible');
            form.remove();
        });
    });
</script>
<?php endif; ?>
