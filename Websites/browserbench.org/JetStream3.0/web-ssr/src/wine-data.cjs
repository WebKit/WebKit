const WINE_DATA = [
  {
    name: "Château Maître 2015",
    winemaker: {
      name: "Château Maître",
      url: "https://www.chateau-maître.com",
      description: "Château Maître, a First Growth Bordeaux, is one of the most famous wines in the world.",
      establishedDate: "1590"
    },
    tastingNotes: "Aromas of blackcurrant, violet, and cedar, with a long, elegant finish.",
    tags: ["elegant", "structured", "classic", "bordeaux"],
    region: {
      name: "Bordeaux",
      province: "Gironde",
      country: "France"
    },
    grapeComposition: [
      { grape: "Cabernet Sauvignon", percentage: 87 },
      { grape: "Merlot", percentage: 8 },
      { grape: "Cabernet Franc", percentage: 3 },
      { grape: "Petit Verdot", percentage: 2 },
    ],
    reviews: [
      { user: "WineEnthusiast", rating: 99, comment: "A legendary vintage. Perfect structure and complexity." },
      { user: "SommelierChoice", rating: 98, comment: "Absolutely stunning. Aromas of blackcurrant, violet, and cedar." },
    ],
    vintage: 2015,
    stock: {
      "750ml": { "quantity": 120, "price": 1500, "salePrice": 1350 },
      "magnum": { "quantity": 24, "price": 3200, "salePrice": null },
      "balthazar": { "quantity": 6, "price": 10000, "salePrice": null }
    }
  },
  {
    name: "Domaine de la Romanée-Comptoir 2016",
    winemaker: {
      name: "Domaine de la Romanée-Comptoir",
      url: "https://www.romanee-comptoir.fr",
      description: "Producer of the world's most sought-after Pinot Noir.",
      establishedDate: "1869"
    },
    tastingNotes: "Ethereal and unforgettable, with notes of red fruit, spice, and earth.",
    tags: ["ethereal", "collectible", "burgundy", "pinot-noir"],
    region: {
      name: "Burgundy",
      province: "Burgundy",
      country: "France"
    },
    grapeComposition: [
      { grape: "Pinot Noir", percentage: 100 },
    ],
    reviews: [
      { user: "CriticalAcclaim", rating: 100, comment: "The pinnacle of Pinot Noir. Ethereal and unforgettable." },
      { user: "CollectorX", rating: 100, comment: "A wine that transcends words. Pure silk." },
    ],
    vintage: 2016,
    stock: {
      "750ml": { "quantity": 50, "price": 25000, "salePrice": null },
      "magnum": { "quantity": 10, "price": 55000, "salePrice": 50000 }
    }
  },
  {
    name: "Screaming Sparrow Cabernet Sauvignon 2018",
    winemaker: {
      name: "Screaming Sparrow Winery",
      url: "https://www.screaming-sparrow-wines.com",
      description: "A cult winery in Napa Valley, known for its extremely limited production Cabernet Sauvignon.",
      establishedDate: "1986"
    },
    tastingNotes: "Rich, opulent, and perfectly balanced, with layers of dark fruit, chocolate, and spice.",
    tags: ["cult", "napa-valley", "powerful", "opulent"],
    region: {
      name: "Napa Valley",
      province: "California",
      country: "USA"
    },
    grapeComposition: [
      { grape: "Cabernet Sauvignon", percentage: 92 },
      { grape: "Merlot", percentage: 5 },
      { grape: "Cabernet Franc", percentage: 3 },
    ],
    reviews: [
      { user: "NapaFan", rating: 99, comment: "Incredibly powerful and elegant at the same time. A Napa icon." },
      { user: "LuxuryWines", rating: 98, comment: "A blockbuster of a wine. Rich, opulent, and perfectly balanced." },
    ],
    vintage: 2018,
    stock: {
      "750ml": { "quantity": 30, "price": 3500, "salePrice": null },
      "magnum": { "quantity": 5, "price": 7500, "salePrice": null }
    }
  },
  {
    name: "Penn & Folders Grange 2017",
    winemaker: {
      name: "Penn & Folders",
      url: "https://www.penn-folders.com",
      description: "One of Australia's most famed and respected winemakers.",
      establishedDate: "1844"
    },
    tastingNotes: "Bold, complex, and built to last, with notes of dark fruit, spice, and a hint of vanilla.",
    tags: ["bold", "australian", "shiraz", "iconic"],
    region: {
      name: "South Australia",
      province: "South Australia",
      country: "Australia"
    },
    grapeComposition: [
      { grape: "Shiraz", percentage: 97 },
      { grape: "Cabernet Sauvignon", percentage: 3 },
    ],
    reviews: [
      { user: "AussieWineLover", rating: 97, comment: "A true Australian legend. Bold, complex, and built to last." },
      { user: "GlobalVino", rating: 98, comment: "Concentrated dark fruit, spice, and a long, savory finish." },
    ],
    vintage: 2017,
    stock: {
      "750ml": { "quantity": 200, "price": 800, "salePrice": 750 },
      "magnum": { "quantity": 50, "price": 1700, "salePrice": null },
      "balthazar": { "quantity": 10, "price": 5000, "salePrice": null }
    }
  },
  {
    name: "Vega Siciliano Único 2011",
    winemaker: {
      name: "Bodegas Vega Siciliano",
      url: "https://www.vega-siciliano.es",
      description: "An iconic Spanish winery in Ribera del Duero, known for its long-aging wines.",
      establishedDate: "1864"
    },
    tastingNotes: "Complex and majestic, with layers of flavor that unfold in the glass, including notes of leather, tobacco, and dark fruit.",
    tags: ["iconic", "spanish", "tempranillo", "complex"],
    region: {
      name: "Ribera del Duero",
      province: "Castile and León",
      country: "Spain"
    },
    grapeComposition: [
      { grape: "Tinto Fino", percentage: 95 },
      { grape: "Cabernet Sauvignon", percentage: 5 },
    ],
    reviews: [
      { user: "SpanishWineGuru", rating: 97, comment: "An iconic Spanish wine with incredible aging potential. Complex and majestic." },
      { user: "EuroCollector", rating: 96, comment: "A benchmark for Ribera del Duero. Layers of flavor that unfold in the glass." },
    ],
    vintage: 2011,
    stock: {
      "750ml": { "quantity": 150, "price": 500, "salePrice": null },
      "magnum": { "quantity": 30, "price": 1100, "salePrice": 1000 }
    }
  },
  // Add 95 more wines to make it a large collection
  ...Array.from({ length: 95 }, (_, i) => ({
    name: `Generic Wine No. ${i + 1}`,
    winemaker: {
      name: `Winery ${i % 10}`,
      url: `http://winery-${i % 10}.com`,
      description: `Description for winery ${i % 10}`,
      establishedDate: `${1900 + (i % 20)}`
    },
    tastingNotes: `A solid wine for the price. Good fruit and structure. Notes of cherry and spice.`,
    tags: ["solid", "value", `generic-tag-${i % 3}`],
    region: {
      name: `Region ${i % 5}`,
      province: `Province ${i % 2}`,
      country: "Country"
    },
    grapeComposition: [
      { grape: "Grape A", percentage: 60 },
      { grape: "Grape B", percentage: 40 },
    ],
    reviews: [
      { user: `User${i}`, rating: 85 + (i % 10), comment: `A solid wine for the price. Good fruit and structure. Review ${i}-1.` },
      { user: `User${i + 100}`, rating: 88 + (i % 5), comment: `Enjoyable, with notes of cherry and spice. Review ${i}-2.` },
    ],
    vintage: 2000 + (i % 20),
    stock: {
      "750ml": { "quantity": i * 10, "price": 50 + i, "salePrice": (i % 3 === 0) ? 45 + i : null },
      "magnum": { "quantity": i, "price": 120 + i * 2, "salePrice": null }
    }
  }))
];


module.exports = {
  WINE_DATA
};