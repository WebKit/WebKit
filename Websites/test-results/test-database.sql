SET search_path TO public;
INSERT INTO results (id, test, build, expected, actual, modifiers, time, is_flaky) VALUES (111, 12345, 23, 'expected1', 'actual1', 'modifiers1', 100, True);
INSERT INTO results (id, test, build, expected, actual, modifiers, time, is_flaky) VALUES (222, 12346, 24, 'expected2', 'actual2', 'modifiers2', 200, True);
INSERT INTO results (id, test, build, expected, actual, modifiers, time, is_flaky) VALUES (333, 12347, 25, 'expected3', 'actual3', 'modifiers3', 333, True);
INSERT INTO results (id, test, build, expected, actual, modifiers, time, is_flaky) VALUES (444, 12348, 26, 'expected4', 'actual4', 'modifiers4', 444, True);

-- older than 90 days, should be deleted by maintenance function
CREATE TABLE "results_partition_2015-01-01" () INHERITS(results);


-- List out inherited databases
SELECT pg_inherits.*, c.relname AS child, p.relname AS parent
FROM
    pg_inherits JOIN pg_class AS c ON (inhrelid=c.oid)
    JOIN pg_class as p ON (inhparent=p.oid);