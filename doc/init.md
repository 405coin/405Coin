Sample init scripts and service configuration for 405Coind
==========================================================

Sample scripts and configuration files for systemd, Upstart and OpenRC
can be found in the contrib/init folder.

    contrib/init/405Coind.service:    systemd service unit configuration
    contrib/init/405Coind.openrc:     OpenRC compatible SysV style init script
    contrib/init/405Coind.openrcconf: OpenRC conf.d file
    contrib/init/405Coind.conf:       Upstart service configuration file
    contrib/init/405Coind.init:       CentOS compatible SysV style init script

Service User
---------------------------------

All three Linux startup configurations assume the existence of a "405Coincore" user
and group.  They must be created before attempting to use these scripts.
The OS X configuration assumes 405Coind will be set up for the current user.

Configuration
---------------------------------

At a bare minimum, 405Coind requires that the rpcpassword setting be set
when running as a daemon.  If the configuration file does not exist or this
setting is not set, 405Coind will shutdown promptly after startup.

This password does not have to be remembered or typed as it is mostly used
as a fixed token that 405Coind and client programs read from the configuration
file, however it is recommended that a strong and secure password be used
as this password is security critical to securing the wallet should the
wallet be enabled.

If 405Coind is run with the "-server" flag (set by default), and no rpcpassword is set,
it will use a special cookie file for authentication. The cookie is generated with random
content when the daemon starts, and deleted when it exits. Read access to this file
controls who can access it through RPC.

By default the cookie is stored in the data directory, but it's location can be overridden
with the option '-rpccookiefile'.

This allows for running 405Coind without having to do any manual configuration.

`conf`, `pid`, and `wallet` accept relative paths which are interpreted as
relative to the data directory. `wallet` *only* supports relative paths.

For an example configuration file that describes the configuration settings,
see `contrib/debian/examples/405Coin.conf`.

Paths
---------------------------------

### Linux

All three configurations assume several paths that might need to be adjusted.

Binary:              `/usr/bin/405Coind`  
Configuration file:  `/etc/405Coincore/405Coin.conf`  
Data directory:      `/var/lib/405Coind`  
PID file:            `/var/run/405Coind/405Coind.pid` (OpenRC and Upstart) or `/var/lib/405Coind/405Coind.pid` (systemd)  
Lock file:           `/var/lock/subsys/405Coind` (CentOS)  

The configuration file, PID directory (if applicable) and data directory
should all be owned by the 405Coincore user and group.  It is advised for security
reasons to make the configuration file and data directory only readable by the
405Coincore user and group.  Access to 405Coin-cli and other 405Coind rpc clients
can then be controlled by group membership.

### Mac OS X

Binary:              `/usr/local/bin/405Coind`  
Configuration file:  `~/Library/Application Support/405CoinCore/405Coin.conf`  
Data directory:      `~/Library/Application Support/405CoinCore`
Lock file:           `~/Library/Application Support/405CoinCore/.lock`

Installing Service Configuration
-----------------------------------

### systemd

Installing this .service file consists of just copying it to
/usr/lib/systemd/system directory, followed by the command
`systemctl daemon-reload` in order to update running systemd configuration.

To test, run `systemctl start 405Coind` and to enable for system startup run
`systemctl enable 405Coind`

### OpenRC

Rename 405Coind.openrc to 405Coind and drop it in /etc/init.d.  Double
check ownership and permissions and make it executable.  Test it with
`/etc/init.d/405Coind start` and configure it to run on startup with
`rc-update add 405Coind`

### Upstart (for Debian/Ubuntu based distributions)

Drop 405Coind.conf in /etc/init.  Test by running `service 405Coind start`
it will automatically start on reboot.

NOTE: This script is incompatible with CentOS 5 and Amazon Linux 2014 as they
use old versions of Upstart and do not supply the start-stop-daemon utility.

### CentOS

Copy 405Coind.init to /etc/init.d/405Coind. Test by running `service 405Coind start`.

Using this script, you can adjust the path and flags to the 405Coind program by
setting the RAPTOREUMD and FLAGS environment variables in the file
/etc/sysconfig/405Coind. You can also use the DAEMONOPTS environment variable here.

### Mac OS X

Copy org.405Coin.405Coind.plist into ~/Library/LaunchAgents. Load the launch agent by
running `launchctl load ~/Library/LaunchAgents/org.405Coin.405Coind.plist`.

This Launch Agent will cause 405Coind to start whenever the user logs in.

NOTE: This approach is intended for those wanting to run 405Coind as the current user.
You will need to modify org.405Coin.405Coind.plist if you intend to use it as a
Launch Daemon with a dedicated 405Coincore user.

Auto-respawn
-----------------------------------

Auto respawning is currently only configured for Upstart and systemd.
Reasonable defaults have been chosen but YMMV.
