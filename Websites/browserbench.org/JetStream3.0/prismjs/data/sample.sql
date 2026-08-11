-- SQL Sample
SELECT
    id,
    name,
    email
FROM
    users
WHERE
    age > 25
ORDER BY
    name;

INSERT INTO products (name, price)
VALUES ('New Product', 99.99);
