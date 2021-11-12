return {
  theme = {
    '@builtins/theme/ng.md',
    -- We don't want to have more than h3 headings in the Table Of Content.
    toc_level = 3,
  },

  site = {
    name = 'WebRTC C++ library',
    home = this.dirname..'index.md',
    logo = this.dirname..'logo.svg',
    map  = this.dirname..'sitemap.md',
    -- Ensure absolute links are rewritten correctly.
    root = this.dirname..'..'
  },

  visibility = { '/...' },

  freshness = {}
}
