<style>
    .webkit-survey-results {
        display: none;
    }
   
    .webkit-survey-results.visible {
        display: block;
    }

    .webkit-survey-results .results {
        position: relative;
        list-style: none;
        padding-left: 0;
    }

    .webkit-survey-results .results > li {
        margin-bottom: 1.7rem;
        position: relative;
    }
    
    .webkit-survey-results .label {
        display: flex;
        flex-direction: row;
        flex-wrap: wrap;
        align-items: flex-end;
    }
    
    .webkit-survey-results .bar {
        display: block;
        height: 7px;
        width: 100%;
        border-radius: 3px;
        background-color: var(--text-color-coolgray);
    }
    
    .webkit-survey-results .winner .bar {
        background-image: linear-gradient(90deg, hsl(198, 100%, 20%) 0%, var(--background-color) 100%);
        background-color: transparent;
    }
    
    .webkit-survey-results .option {
        flex-shrink: 1;
        align-self: end;
        overflow: visible;
    }
    
    .webkit-survey-results .percentage {
        flex-shrink: 1;
        width: 3.8ch;
        font-weight: 700;
        margin-left: 1ch;
        align-self: end;
    }

    .webkit-survey-results .percentage.right {
        text-align: right;
    }
</style>
<?php 
    $SurveyResults = WebKit_Survey::calculate_results();
    foreach ($SurveyResults->survey as $id => $Entry): $total = isset($Entry->scores['total']) ? $Entry->scores['total'] : 1; 
    
    $results_classes = ['webkit-survey-results'];
    if ($SurveyResults->status == 'closed' || $SurveyResults->results == 'visible' || WebKit_Survey::responded()) 
        $results_classes[] = 'visible';
?>
<div class="<?php esc_attr_e(join(' ', $results_classes)); ?>">
    <h3><?php echo $Entry->question; ?></h3>
    <ul class="results">
        <?php 
        foreach ($Entry->responses as $value => $option): 
            if ($option == "Other:") {
                $option = "Other";
            }
            $score = isset($Entry->scores[$value]) ? $Entry->scores[$value] : 0; 
            $percentage = $score * 100 / $total;
        ?>
        <li<?php if ($SurveyResults->winner == $value) echo ' class="winner"'; ?>>
            <div class="label">
                <div class="option" style="min-width: calc(<?php esc_attr_e($percentage); ?>% - 4.8ch);"><?php echo esc_html($option); ?></div>
                <div class="percentage"><?php echo number_format($percentage, 0); ?>%</div>
            </div>
            <div class="bar" style="width: <?php echo esc_attr(max(1,$percentage)); ?>%">&nbsp;</div>
        </li>
    <?php endforeach; ?>
    </ul>
<?php endforeach; ?>
</div>
