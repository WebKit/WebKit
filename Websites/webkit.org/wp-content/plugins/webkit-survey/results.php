<style>
    .survey-results {
        position: relative;
        list-style: none;
        padding-left: 0;
    }

    .survey-results > li {
        margin-bottom: 1.7rem;
        position: relative;
    }
    
    .label {
        display: flex;
        flex-direction: row;
        flex-wrap: wrap;
        align-items: flex-end;
    }
    
    .bar {
        display: block;
        height: 0.7rem;
        width: 100%;
        border-radius: 3px;
        background-color: var(--text-color-coolgray);
    }
    
    .winner .bar {
        background-image: linear-gradient(90deg, hsl(198, 100%, 20%) 0%, var(--background-color) 100%);
        background-color: transparent;
    }
    
    .option {
        flex-shrink: 1;
        align-self: end;
        overflow: visible;
    }
    
    .percentage {
        flex-shrink: 1;
        width: 4rem;
        font-weight: 700;
        margin-left: 1rem;
        align-self: end;
    }

    .percentage.right {
        text-align: right;
    }
</style>
<?php 
    $SurveyResults = WebKit_Survey::calculate_results();
    foreach ($SurveyResults->survey as $id => $Entry): $total = isset($Entry->scores['total']) ? $Entry->scores['total'] : 1; 
?>
    <h3><?php echo $Entry->question; ?></h3>
    <ul class="survey-results">
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
                <div class="option" style="min-width: calc(<?php esc_attr_e($percentage); ?>% - 5rem);"><?php echo esc_html($option); ?></div>
                <div class="percentage"><?php echo number_format($percentage, 0); ?>%</div>
            </div>
            <div class="bar" style="width: <?php echo esc_attr(max(1,$percentage)); ?>%">&nbsp;</div>
        </li>
    <?php endforeach; ?>
    </ul>
<?php endforeach; ?>
