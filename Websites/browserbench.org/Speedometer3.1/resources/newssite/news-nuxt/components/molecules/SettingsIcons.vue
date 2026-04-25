<script lang="js">
import styles from "news-site-css/dist/icons-group.module.css";
export default {
  props: {
      callback: Function,
      id: String,
  },
  data () {
    return {
      styles
    }
  }
}
</script>

<template>
    <div :class="styles['icons-group']">
        <ul :class="styles['icons-group-list']">
            <li :class="styles['icons-group-item']">
                <button :id="`${id}-a11y`" @click="callback">
                    <div :class="[styles['group-icon'], styles['group-icon-medium']]">
                        <A11yIcon />
                    </div>
                </button>
            </li>
        </ul>
    </div>
</template>
