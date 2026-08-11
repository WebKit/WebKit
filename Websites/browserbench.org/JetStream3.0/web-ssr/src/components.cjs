const React = require('react');
const { countryFlags } = require('./country-flags.cjs');

const Review = ({ review }) => (
    <div className="review">
        <h4>{review.user} ({review.rating}/100)</h4>
        <p>{review.comment}</p>
    </div>
);

const Grape = ({ grape }) => (
    <li>
        <a href={`/list/?grape=${grape.grape}`}>{grape.grape}</a> ({grape.percentage}%)
        <div style={{
            display: 'inline-block',
            border: '1px solid #ccc',
            width: '100px',
            height: '10px',
            marginLeft: '10px',
            verticalAlign: 'middle'
        }}>
            <div style={{
                width: `${grape.percentage}%`,
                height: '100%',
                backgroundColor: '#663399'
            }}></div>
        </div>
    </li>
);

const GrapeComposition = ({ composition }) => (
    <ul className="grape-composition">
        {composition.map(g => (
            <Grape key={g.grape} grape={g} />
        ))}
    </ul>
);

const WineName = ({ name, hasHighRating }) => (
    <h2>{name} {hasHighRating && '⭐'}</h2>
);

const Winemaker = ({ winemaker }) => (
    <div className="winemaker">
        <h3><a href={winemaker.url}>{winemaker.name}</a></h3>
        <p>{winemaker.description}</p>
        <p>Established: {winemaker.establishedDate}</p>
    </div>
);

const Region = ({ region }) => (
    <h3>
        {region.name} - <a href={`/list/?country=${region.country}&province=${region.province}`}>{region.province}</a>,
        <a href={`/list/?country=${region.country}`}>{region.country}</a>
        {countryFlags[region.country]}
    </h3>
);

const Tag = ({ tag }) => (
    <li key={tag}><a href={`/list/?tag=${tag}`}>{tag}</a></li>
);

const Tags = ({ tags }) => (
    <ul className="tags">
        {tags.map(t => (
            <Tag key={t} tag={t} />
        ))}
    </ul>
);

const Vintage = ({ vintage }) => (
    <h4>Vintage: {vintage}</h4>
);

const getSaleInfo = (price, salePrice) => {
    const onSale = salePrice && salePrice < price;
    const percentageDiff = onSale ? Math.round(((price - salePrice) / price) * 100) : 0;
    return { onSale, percentageDiff };
};

const getPriceRating = (price) => {
    if (price < 100) return "€";
    if (price < 1000) return "€€";
    return "€€€";
};

const PriceRating = ({ price }) => (
    <span>{getPriceRating(price)}</span>
);

const Price = ({ price, salePrice }) => {
    const onSale = salePrice && salePrice < price;

    if (onSale) {
        return (
            <span>
                <span style={{ textDecoration: 'line-through' }}>${price}</span> ${salePrice}
            </span>
        );
    }
    return <span>${price}</span>;
};

const Bottle750 = ({ bottle }) => {
    const { quantity, price, salePrice } = bottle;
    const { onSale, percentageDiff } = getSaleInfo(price, salePrice);

    return (
        <li>
            750ml: {quantity} {quantity < 10 && '(Low stock)'} - <Price price={price} salePrice={salePrice} /> <PriceRating price={price} />
            {onSale && <span>❗({percentageDiff}% off)</span>}
        </li>
    );
};

const BottleMagnum = ({ bottle }) => {
    const { quantity, price, salePrice } = bottle;
    const { onSale, percentageDiff } = getSaleInfo(price, salePrice);

    return (
        <li>
            Magnum: {quantity} {quantity < 10 && '(Low stock)'} - <Price price={price} salePrice={salePrice} /> <PriceRating price={price} />
            {onSale && <span>❗({percentageDiff}% off)</span>}
        </li>
    );
};

const BottleBalthazar = ({ bottle }) => {
    const { quantity, price, salePrice } = bottle;
    const { onSale, percentageDiff } = getSaleInfo(price, salePrice);

    return (
        <li>
            Balthazar: {quantity} {quantity < 10 && '(Low stock)'} - <Price price={price} salePrice={salePrice} /> <PriceRating price={price} />
            {onSale && <span>❗({percentageDiff}% off)</span>}
        </li>
    );
};

const Stock = ({ stock }) => (
    <div className="stock">
        <h4>Stock</h4>
        {stock && Object.values(stock).some(s => s.quantity > 0) ? (
            <ul>
                {stock["750ml"] && <Bottle750 bottle={stock["750ml"]} />}
                {stock["magnum"] && <BottleMagnum bottle={stock["magnum"]} />}
                {stock["balthazar"] && <BottleBalthazar bottle={stock["balthazar"]} />}
            </ul>
        ) : (
            <p>Out of stock</p>
        )}
    </div>
);

const Wine = ({ wine }) => (
    <div className="wine-card">
        <WineName name={wine.name} hasHighRating={wine.reviews.some(r => r.rating > 90)} />
        <Winemaker winemaker={wine.winemaker} />
        <Region region={wine.region} />
        <Vintage vintage={wine.vintage} />
        <h4>Tasting Notes</h4>
        <p>{wine.tastingNotes}</p>
        <Tags tags={wine.tags} />
        <h4>Grape Composition</h4>
        <GrapeComposition composition={wine.grapeComposition} />
        <Stock stock={wine.stock} />
        <h4>Reviews</h4>
        {wine.reviews.map((review, i) => (
            <Review key={i} review={review} />
        ))}
    </div>
);

const WineList = ({ wines }) => (
    <div className="wine-list">
        <h1>Complex Wine List</h1>
        {wines.map((wine, i) => (
            <Wine key={i} wine={wine} />
        ))}
    </div>
);

module.exports = {
    Review,
    Grape,
    GrapeComposition,
    WineName,
    Winemaker,
    Region,
    Tag,
    Tags,
    Vintage,
    Price,
    PriceRating,
    Bottle750,
    BottleMagnum,
    BottleBalthazar,
    Stock,
    Wine,
    WineList,
};
