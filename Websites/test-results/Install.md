# Checking Out the Code and Installing Required Applications

Note: These instructions assume you're using Mac OS X Mountain Lion as the host server, and assume that we're installing
this application at `/Volumes/Data/test-results`.

1. Install Server (DO NOT launch the Server app)
2. Install Xcode with command line tools (only needed for svn)
3. `svn co https://svn.webkit.org/repository/webkit/trunk/Websites/test-results /Volumes/Data/test-results`

# Configuring Apache

Always use apachectl instead of the Server App to start or stop Apache.
 - Starting httpd: `sudo apachectl stop`
 - Stopping httpd: `sudo apachectl restart`

## Edit /private/etc/apache2/httpd.conf

1. Uncomment `"LoadModule php5_module libexec/apache2/libphp5.so"`
2. Update ServerAdmin to `rniwa@apple.com`
2. Set ServerName.
3. Change DocumentRoot to `/Volumes/Data/test-results/public/`
4. Change the directives for the document root and / to point to `/Volumes/Data/test-results/public/`
5. Add Add the following directives to enable zlib compression and MultiViews:

        Options Indexes MultiViews
        php_flag zlib.output_compression on

6. Delete or comment out directives on CGI-Executables
7. Add the following directives to enable gzip:

        <IfModule mod_deflate.c>
            AddOutputFilterByType DEFLATE text/html text/xml text/plain application/json application/xml application/xhtml+xml
        </IfModule>

Note: If you've accidentally turned on the Server app, httpd.conf is located at `/Library/Server/Web/Config/apache2/` instead.
Delete the Web Sharing related stuff and include `/private/etc/apache2/httpd.conf` at the very end.

By default, the Apache error log will be located at `/private/var/log/apache2/error_log`.


# Protecting the Administrative Pages to Prevent Execution of Arbitrary Code

By default, the application gives the administrative privilege to everyone. Anyone can add, remove, or edit tests,
builders, and other entities in the database and may even execute arbitrary JavaScript on the server via aggregators.

We recommend protection via Digest Auth on https connection.

Generate a password file via `htdigest -c <path> <realm> <username>`, and then add the following directive:

        <Directory "/Volumes/Data/test-results/public/admin/">
        	AuthType Digest
        	AuthName "<realm>"
        	AuthDigestProvider file
        	AuthUserFile "<path>"
        	Require valid-user
        </Directory>

where <realm> is replaced with the realm of your choice.


# Configuring PostgreSQL

1. Create database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/initdb /Volumes/Data/test-results/database`
2. Start database:
   `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_ctl -D /Volumes/Data/test-results/database
   -l logfile -o "-k /Volumes/Data/test-results/database" start`

## Creating a Database and a User

1. Create a database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/createdb test-results-db -h localhost`
2. Create a user: `/Applications/Server.app/Contents/ServerRoot/usr/bin/createuser -P -S -e test-results-user -h localhost`
3. Connect to database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/psql test-results-db -h localhost`
4. Grant all permissions to the new user: `grant all privileges on database "test-results-db" to "test-results-user";`
5. Update config.json.

## Initializing the Database

Run `init-database.sql` in psql as `test-results-user`:
`/Applications/Server.app/Contents/ServerRoot/usr/bin/psql test-results-db -h localhost --username test-results-user -f init-database.sql`
