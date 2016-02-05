# Checking Out the Code and Installing Required Applications

The instructions assume you're using Mac OS X as the host server and installing this application at `/Volumes/Data/perf.webkit.org`.

You can choose between using Server.app or install the required tools separately

1. Install Server.app (if you don't want to use Server.app, install PostgreSQL: http://www.postgresql.org/download/macosx/)
2. Install node.
3. Install Xcode with command line tools (only needed for svn)
4. `svn co https://svn.webkit.org/repository/webkit/trunk/Websites/perf.webkit.org /Volumes/Data/perf.webkit.org`
5. Inside `/Volumes/Data/perf.webkit.org`, run `npm install pg` and `mkdir -m 755 public/data/`

# Testing Local UI Changes with Production Data

The front end has the capability to pull data from a production server without replicating the database locally on OS X (Yosemite and later).
To use this feature, modify `config.json`'s `remoteServer` entry so that "remoteServer.url" points to your production server,
and "remoteServer.basicAuth" specifies the username and the password that is used by the production sever.

Remove "basicAuth" entry for production servers that doesn't require a basic authentication (e.g. perf.webkit.org).

```json
{
    "url": "http://perf.webkit.org",
    "basicAuth": {
        "username": "webkitten",
        "password": "webkitten's secret password"            
    }
}
```

Then run `tools/remote-cache-server.py start`. This launches a httpd server on port 8080.

The script caches remote server's responses under `public/data/remote-cache` and never revalidates them (to allow offline work).
If you needed the latest content, delete caches stored in this directory by running `tools/remote-cache-server.py reset`.


# Configuring Apache

## Instructions if you're using Server.app

 - Enable PHP web applications
 - Go to Server Website / Store Site Files In, change it to `/Volumes/Data/perf.webkit.org/public/`
 - Go to Server Website / Edit advanced settings, enable Allow overrides using .htaccess files
 - httpd config file is located at `/Library/Server/Web/Config/apache2/sites/0000_any_80.conf` (and/or 0000_any_`PORT#`.conf)

## Instructions if you're not using Server.app

 - Edit /private/etc/apache2/httpd.conf

     1. Change DocumentRoot to `/Volumes/Data/perf.webkit.org/public/`
     2. Uncomment `LoadModule php5_module libexec/apache2/libphp5.so`
     3. Uncomment `LoadModule rewrite_module libexec/apache2/mod_rewrite.so`
     4. Uncomment `LoadModule deflate_module libexec/apache2/mod_deflate.so`

 - In Mavericks and later, copy php.ini to load pdo_pgsql.so pgsql.so.
    `sudo cp /Applications/Server.app/Contents/ServerRoot/etc/php.ini /etc/`
 - In El Capitan and later, comment out the `LockFile` directive in `/private/etc/apache2/extra/httpd-mpm.conf`
   since the directive has been superseded by `Mutex` directive.

## Starting Apache

You can use apachectl to start/stop/restart apache server from the command line:

- Starting httpd: `sudo apachectl start`
- Stopping httpd: `sudo apachectl stop`
- Restarting httpd: `sudo apachectl restart`

The apache logs are located at `/private/var/log/apache2`.

## Production Configurations

 1. Update ServerAdmin to your email address
 2. Add the following directives to enable gzip:

        <IfModule mod_deflate.c>
            AddOutputFilterByType DEFLATE text/html text/xml text/plain application/json application/xml application/xhtml+xml
        </IfModule>

 3. Add the following directives to enable zlib compression and MultiViews on DocumentRoot (perf.webkit.org/public):

        Options Indexes MultiViews
        php_flag zlib.output_compression on

### Protecting the Administrative Pages to Prevent Execution of Arbitrary Code

By default, the application gives the administrative privilege to everyone. Anyone can add, remove, or edit tests,
builders, and other entities in the database.

We recommend protection via Digest Auth on https connection.

Generate a password file via `htdigest -c <path> <realm> <username>`, and then create admin/.htaccess with the following directives
where `<Realm>` is replaced with the realm of your choice, which will be displayed on the username/password input box:

```
AuthType Digest
AuthName "<Realm>"
AuthDigestProvider file
AuthUserFile "<Realm>"
Require valid-user
```

# Configuring PostgreSQL

Run the following command to setup a Postgres server at `/Volumes/Data/perf.webkit.org/PostgresSQL` (or wherever you'd prefer):
`python ./setup-database.py /Volumes/Data/perf.webkit.org/PostgresSQL`

It automatically retrieves the database name, the username, and the password from `config.json`.

## Starting PostgreSQL

The setup script automatically starts the database but you may need to run the following command to manually start the database after reboot.

- Starting the database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_ctl -D /Volumes/Data/perf.webkit.org/PostgresSQL -l /Volumes/Data/perf.webkit.org/PostgresSQL/logfile -o "-k /Volumes/Data/perf.webkit.org/PostgresSQL" start`
- Stopping the database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_ctl -D /Volumes/Data/perf.webkit.org/PostgresSQL -l /Volumes/Data/perf.webkit.org/PostgresSQL/logfile -o "-k /Volumes/Data/perf.webkit.org/PostgresSQL" start`

## Initializing the Database

Run `database/init-database.sql` in psql as `webkit-perf-db-user`:
`/Applications/Server.app/Contents/ServerRoot/usr/bin/psql webkit-perf-db -h localhost --username webkit-perf-db-user -f init-database.sql`

## Making a Backup and Restoring

- Backing up the database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_dump -h localhost --no-owner -f <filepath> webkit-perf-db | gzip > backup.gz`
- Restoring the database: `gunzip -c backup.gz | /Applications/Server.app/Contents/ServerRoot/usr/bin/psql webkit-perf-db -h localhost --username webkit-perf-db-user`
