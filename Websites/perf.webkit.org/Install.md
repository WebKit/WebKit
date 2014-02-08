# Checking Out the Code and Installing Required Applications

Note: These instructions assume you're using Mac OS X Mountain Lion as the host server, and assume that we're installing
this application at `/Volumes/Data/WebKitPerfMonitor`.

1. Install Server (DO NOT launch the Server app)
2. Install node.
3. Install Xcode with command line tools (only needed for svn)
4. `svn co https://svn.webkit.org/repository/webkit/trunk/Websites/perf.webkit.org /Volumes/Data/WebKitPerfMonitor`
5. Inside `/Volumes/Data/WebKitPerfMonitor`, run `npm install pg`.


# Configuring Apache

Don't use the Server App to start or stop Apache. It does weird things to httpd configurations. Use apachectl instead:
 - Starting httpd: `sudo apachectl stop`
 - Stopping httpd: `sudo apachectl restart`

## Edit /private/etc/apache2/httpd.conf

1. Update ServerAdmin to your email address
2. Change DocumentRoot to `/Volumes/Data/WebKitPerfMonitor/public/`
3. Uncomment `"LoadModule php5_module libexec/apache2/libphp5.so"`
4. Modify the directives for the document root and / to allow overriding `"All"`
5. Delete directives on CGI-Executables
6. Add the following directives to enable gzip:
    
        <IfModule mod_deflate.c>
            AddOutputFilterByType DEFLATE text/html text/xml text/plain application/json application/xml application/xhtml+xml
        </IfModule>

7. Add the following directives to enable zlib compression and MultiViews on WebKitPerfMonitor/public:

        Options Indexes MultiViews
        php_flag zlib.output_compression on

Note: If you've accidentally turned on the Server app, httpd.conf is located at `/Library/Server/Web/Config/apache2/` instead.
Delete the Web Sharing related stuff and include `/private/etc/apache2/httpd.conf` at the very end.

The log is located at `/private/var/log/apache2`.


# Protecting the Administrative Pages to Prevent Execution of Arbitrary Code

By default, the application gives the administrative privilege to everyone. Anyone can add, remove, or edit tests,
builders, and other entities in the database and may even execute arbitrary JavaScript on the server via aggregators.

We recommend protection via Digest Auth on https connection.

Generate a password file via `htdigest -c <path> <realm> <username>`, and then create admin/.htaccess with:

	AuthType Digest
	AuthName "<Realm>"
	AuthDigestProvider file
	AuthUserFile "<Realm>"
	Require valid-user

where <Realm> is replaced with the realm of your choice.


# Configuring PostgreSQL

1. Create database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/initdb /Volumes/Data/WebKitPerfMonitor/PostgresSQL`
2. Start database:
   `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_ctl -D /Volumes/Data/WebKitPerfMonitor/PostgresSQL
   -l logfile -o "-k /Volumes/Data/WebKitPerfMonitor/PostgresSQL" start`

## Creating a Database and a User

1. Create a database: `createdb webkit-perf-db -h localhost`
2. Create a user: `createuser -P -S -e webkit-perf-db-user -h localhost`
3. Connect to database: `psql webkit-perf-db -h localhost`
4. Grant all permissions to the new user: `grant all privileges on database "webkit-perf-db" to "webkit-perf-db-user";`
5. Update database/config.json.

## Initializing the Database

Run `database/init-database.sql` in psql as `webkit-perf-db-user`:
`psql webkit-perf-db -h localhost --username webkit-perf-db-user -f database/init-database.sql`
