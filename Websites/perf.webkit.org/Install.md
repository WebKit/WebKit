# Checking Out the Code and Installing Required Applications

The instructions assume you're using Mac OS X (Mavericks for Server.app case and Mountain Lion without Server.app) as the
host server, and assume that we're installing this application at `/Volumes/Data/WebKitPerfMonitor`.

You can choose between using Server.app or install the required tools separately

1. Install Server.app (if you don't want to use Server.app, install PostgreSQL: http://www.postgresql.org/download/macosx/)
2. Install node.
3. Install Xcode with command line tools (only needed for svn)
4. `svn co https://svn.webkit.org/repository/webkit/trunk/Websites/perf.webkit.org /Volumes/Data/WebKitPerfMonitor`
5. Inside `/Volumes/Data/WebKitPerfMonitor`, run `npm install pg`.


# Configuring Apache

You can use apachectl to start/stop/restart apache server from the command line:

 - Starting httpd: `sudo apachectl start`
 - Stopping httpd: `sudo apachectl stop`
 - Restarting httpd: `sudo apachectl restart`

## Instructions if you're using Server.app

 - Enable PHP web applications
 - Go to Server Website / Store Site Files In, change it to /Volumes/Data/WebKitPerfMonitor/public/`
 - Go to Server Website / Edit advanced settings, enable Allow overrides using .htaccess files

## Instructions if you're not using Server.app

 - Edit /private/etc/apache2/httpd.conf

     1. Change DocumentRoot to `/Volumes/Data/WebKitPerfMonitor/public/`
     2. Uncomment `"LoadModule php5_module libexec/apache2/libphp5.so"`
     3. Modify the directives for the document root and / to allow overriding `"All"`
     4. Delete directives on CGI-Executables

## Common directives for the related apache config file

  httpd config file is located at:

    - With Server.app: /Library/Server/Web/Config/apache2/sites/0000_any_80.conf (and/or 0000_any_`PORT#`.conf)
    - Without: /private/etc/apache2/httpd.conf

 1. Update ServerAdmin to your email address
 2. Add the following directives to enable gzip:

        <IfModule mod_deflate.c>
            AddOutputFilterByType DEFLATE text/html text/xml text/plain application/json application/xml application/xhtml+xml
        </IfModule>

 3. Add the following directives to enable zlib compression and MultiViews on WebKitPerfMonitor/public:

        Options Indexes MultiViews
        php_flag zlib.output_compression on

The apache logs are located at `/private/var/log/apache2`.


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

where <Realm> is replaced with the realm of your choice, which will be displayed on the username/password input box.


# Configuring PostgreSQL

1. Create database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/initdb /Volumes/Data/WebKitPerfMonitor/PostgresSQL`
2. Start database:
   `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_ctl -D /Volumes/Data/WebKitPerfMonitor/PostgresSQL
   -l logfile -o "-k /Volumes/Data/WebKitPerfMonitor/PostgresSQL" start`

## Creating a Database and a User

The binaries located in PostgreSQL's directory, or if you're using Server.app in /Applications/Server.app/Contents/ServerRoot/usr/bin/

1. Create a database: `createdb webkit-perf-db -h localhost`
2. Create a user: `createuser -P -S -e webkit-perf-db-user -h localhost`
3. Connect to database: `psql webkit-perf-db -h localhost`
4. Grant all permissions to the new user: `grant all privileges on database "webkit-perf-db" to "webkit-perf-db-user";`
5. Update database/config.json.

## Initializing the Database

Run `database/init-database.sql` in psql as `webkit-perf-db-user`:
`psql webkit-perf-db -h localhost --username webkit-perf-db-user -f database/init-database.sql`
