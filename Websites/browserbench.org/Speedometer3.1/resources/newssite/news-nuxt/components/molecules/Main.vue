<script lang="js">
import styles from "news-site-css/dist/layout.module.css";
export default {
  data () {
    return {
      styles
    }
  }
}
</script>

<template>
    <main :class="styles['page-main']">
        <slot />
    </main>
</template>
