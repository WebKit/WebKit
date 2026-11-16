
# How to configure the server for allowing secure SFTP uploads

## Initial setup

### 1. Install openssh server

```
sudo yum install openssh-server
```

### 2. Create the system group

We will use a system group named `Buildbot-Upload-Users` to identify all
the system users that are allowed to upload build products

```
sudo groupadd --system Buildbot-Upload-Users
```

### 3. Configure the SSH server as follows

```diff
--- a/etc/ssh/sshd_config
+++ b/etc/ssh/sshd_config
@@ -14,7 +14,7 @@
 # SELinux about this change.
 # semanage port -a -t ssh_port_t -p tcp #PORTNUMBER
 #
-#Port 22
+Port 6214
 #AddressFamily any
 #ListenAddress 0.0.0.0
 #ListenAddress ::
@@ -76,8 +76,8 @@ ChallengeResponseAuthentication no
 #KerberosUseKuserok yes

 # GSSAPI options
-GSSAPIAuthentication yes
-GSSAPICleanupCredentials no
+#GSSAPIAuthentication yes
+#GSSAPICleanupCredentials no
 #GSSAPIStrictAcceptorCheck yes
 #GSSAPIKeyExchange no
 #GSSAPIEnablek5users no
@@ -137,3 +137,11 @@ Subsystem  sftp    /usr/libexec/openssh/sftp-server
 #      AllowTcpForwarding no
 #      PermitTTY no
 #      ForceCommand cvs server
+
+Match Group Buildbot-Upload-Users
+    X11Forwarding no
+    AllowTcpForwarding no
+    PasswordAuthentication no
+    AuthorizedKeysFile /etc/ssh/buildbot_authorized_keys/%u
+    ForceCommand internal-sftp
+    ChrootDirectory /var/chroot/buildbot-built-products
```

In this diff we did the following:

 - Changed the default SSH port from `22` to any other random number (to make it more secure, and avoid the spam of failed log attempts of bots that try to bruteforce passwords on SSH servers)
 - Disabled `GSSAPI` (it makes the login process more slow and we don't need it)
 - Added a `Match` block at the end that will apply to all users members of the UNIX group `Buildbot-Upload-Users`
   - In this match block: we disable forwarding and password auth, we defined where the `authorized_key` file is for each user and we force `internal-sftp` inside a chroot directory


### 4. Create the chroot directory

```
sudo mkdir -p /var/chroot/buildbot-built-products
sudo chown root:root /var/chroot/buildbot-built-products
# Note: the main directory for the chroot has to be owned by root, so the users can't write to it.
```

### 5. Create the directory for storing the users authorized_keys files

```
sudo mkdir -p /etc/ssh/buildbot_authorized_keys
```

### 6. Restart the SSH service

```
sudo systemctl restart openssh-server.service
```

## Adding new users

### 7. Create a new SSH key pair for the bot

We do this on the bot machine (not in the SSH server machine) and we don't
set a password for the SSH key, otherwise we can't automate the upload.

```
# Note: we run this command on the bot machine (not on the SSH server).
ssh-keygen -f ~/key-for-uploads
# No password when asked
# Copy the contents of the public key ~/key-for-uploads.pub to the clipboard
```

### 8. Create the user, directories and add the SSH public key in the server

We create the user on the server running the SSH service where the uploads should be sent:

```
# This command are executed on the SSH server, we use variables to make it easier scripting it.
# Add user name "GTK-Linux-64-bit-Release-Build" and create the required directory for its uploads
export NEWBUILDBOTUSER=GTK-Linux-64-bit-Release-Build
sudo adduser --system --gid Buildbot-Upload-Users --shell /sbin/nologin --home-dir / --no-create-home "${NEWBUILDBOTUSER}"
sudo mkdir "/var/chroot/buildbot-built-products/${NEWBUILDBOTUSER}"
sudo chown "${NEWBUILDBOTUSER}" "/var/chroot/buildbot-built-products/${NEWBUILDBOTUSER}"
```

Now add the SSH public key that we created on the bot machine (on `7.` above) in the server:

```
# Finally save the contents of ~/key-for-uploads.pub (from the clipoard) in its respective file on the SSH server
cat file-from-bot/key-for-uploads.pub > "/etc/ssh/buildbot_authorized_keys/${NEWBUILDBOTUSER}"
```

### 9. Test it

Run on the bot:

```
python3 Tools/CISupport/Shared/transfer-archive-via-sftp \
	--log-level debug \
	--server-address sshserver.webkit.org \
	--server-port 6214 \
	--remote-dir GTK-Linux-64-bit-Release-Build \
	--user-name GTK-Linux-64-bit-Release-Build \
	--ssh-key ~/key-for-uploads \
	--remote-file test_upload_123.zip  \
	WebKitBuild/release.zip
```

If everything worked as expected the zip file should appear on the server at directory:

```
/var/chroot/buildbot-built-products/GTK-Linux-64-bit-Release-Build
```

### 10. Debug problems


If it doesn't work try to check the syslog of the server to see if openssh-server
is saying something. This command may be useful

```
sudo journalctl -f -u openssh-server.service
```

Another option is stopping the ssh server and launching it manually in the foreground
to get the logs printed on stdout

```
sudo systemctl stop openssh-server
sudo  /usr/sbin/sshd -D -e
```

### 11. Final tips:

* Semi-automating adding users

Adding new users is repeating the steps of `8.` (above) for each new user, so it is a good
idea to create a simple shell script that accepts as input the `bot_name` and the contents
of the bot `ssh public key` and runs those comands creating the system user, the directories
and setting the `authorized_key` file for it.

* Running the script on production

To deploy this on the bots in production instead of passing the server configuration
and path to the ssh keys via the command line we simply pass the path to a `json` file
that contains the remote configuration. Example:

```
python3 Tools/CISupport/Shared/transfer-archive-via-sftp \
	--remote-config-file ../../remote-built-product-upload-config.json \
	--user-name WithProperties("%(buildername)s") \
	--remote-dir WithProperties("%(buildername)s") \
	--remote-file WithProperties("%(archive_revision)s.zip") \
	WithProperties("WebKitBuild/%(configuration)s.zip")
```

And the config file `remote-built-product-upload-config.json` that is stored in the bot
contains something like this:

```
{
"server_name": "webkit.org",
"server_address": "sshserver.webkit.org",
"server_port": "6214",
"ssh_key": "/home/buildbot/key-for-uploads"
}
```

**Note**: the tool supports (and is recommended) that the ssh private key is not passed as a path,
but instead the binary contents of it are defined in the `json` config file as base64 data.
Run the command:

```
# Get the contents of the SSH private key as base64 encoded data (without new lines)
# On Linux
cat /home/buildbot/key-for-uploads | base64 -w0
# On Mac
cat /home/buildbot/key-for-uploads | base64
```

And paste the base64 encoded data inside the field `ssh_key` in the `json` file:

```
{
"server_name": "webkit.org",
"server_address": "sshserver.webkit.org",
"server_port": "6214",
"ssh_key": "LS0tLS1CRUdJTiBPUEVOU1NIIFBSSVZBVEUgS0VZLS0tLS0[..REDACTED..]FURSBLRVktLS0tLQo="
}
```

Then delete the key from the disk:

```
rm -f /home/buildbot/key-for-uploads
```

This way the only file storing sensitive data on the bot is that config file,
the SSH private key not longer exist on the disk outside of the `json` file.
This helps to make backups easier and restoring the bot config from scratch.

### 12. Limit SSH access by IP address

As an extra security measure it if the machines uploading the data have known fixed
public IP address then it is advisable to limit access to the SSH service to only
this IPs.

This can be done as follows:

1.  By default, deny all hosts.

```
# cat /etc/hosts.deny
sshd : ALL
```

2. Then only allow specific IP address. Example:

```
# cat /etc/hosts.allow
sshd : 192.168.0.0/24
sshd : 127.0.0.1
sshd : [::1]
sshd : 52.92.165.64
```

Another option is using `iptables` (firewall) to limit to specific IP address
connecting with the SSH port.

More info: https://unix.stackexchange.com/questions/406245/limit-ssh-access-to-specific-clients-by-ip-address
