try {
      JSON.stringify("123".padStart(1073741823))
} catch (e) {}
