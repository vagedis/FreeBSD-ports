<?php
/*
	nut.inc

	part of pfSense (http://www.pfSense.org/)
	Copyright (C) 2015 ESF, LLC
	Copyright (C) 2016 Denny Page
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/


require_once("config.inc");
require_once("functions.inc");
require_once("util.inc");
require_once("service-utils.inc");

define("NUT_ETC", "/usr/local/etc/nut");
define("NUT_MONFILE", NUT_ETC . "/upsmon.conf");
define("NUT_UPSFILE", NUT_ETC . "/ups.conf");
define("NUT_UPSDFILE", NUT_ETC . "/upsd.conf");
define("NUT_USERSFILE", NUT_ETC . "/upsd.users");
define("NUT_RCFILE", "/usr/local/etc/rc.d/nut.sh");

$shortcut_section = "nut";

function nut_check_var_db() {
	/*
		Directory /var/db/nut is actually owned by the nut package itself, however
		there are a couple of circumstance in which it might not exist:

		The prior nut package incorrectly assumed ownership of the directory and
		removed it upon deinstall. If the pfSense nut package was reinstalled, but
		the nut package itself was not, the directory will be missing.

		The other situation in which the directory will be missing is when /var is
		a ram based filesystem. In this case, the contents of /var are lost at each
		reboot and must be recreated.
	*/

	$d = "/var/db/nut";
	if (!is_dir($d)) {
		mkdir($d, 0750, true);
		safe_mkdir($d, 0750);
		chown($d, "uucp");
		chgrp($d, "uucp");
	}
}

function nut_write_rcfile($driver) {
	nut_check_var_db();

	$start = "echo starting NUT";
	$start .= "\n	/usr/bin/killall -q -9 upsmon";
	$start .= "\n	/usr/bin/killall -q -9 upsd";
	$start .= "\n	/usr/bin/killall -q -9 upsdrvctl";
	if (isset($driver)) {
		$start .= "\n	/usr/bin/killall -q -9 $driver";

		$start .= "\n	/usr/local/sbin/upsdrvctl start &";
		$start .= "\n	/usr/local/sbin/upsd -u root";
	}
	$start .= "\n	/usr/local/sbin/upsmon";
	$start .= "\n	return 0";

	$stop = "echo stopping NUT";
	$stop .= "\n	/usr/bin/killall -q upsmon";
	if (isset($driver)) {
		$stop .= "\n	/usr/bin/killall -q upsd";
		$stop .= "\n	/usr/bin/killall -q upsdrvctl";
		$stop .= "\n	/usr/bin/killall -q $driver";
	}
	$stop .= "\n	return 0";

	write_rcfile(array(
		"file" => "nut.sh",
		"start" => $start,
		"stop" => $stop
		)
	);
}


function nut_write_file($file, $content) {
	$fd = fopen($file, "w");
	if (!$fd) {
		log_error("ERROR: Could not open {$file} for writing");
		exit;
	}
	chmod($file, 0640);
	fwrite($fd, $content);
	fclose($fd);
}


function nut_sync_config() {
	global $config;

	if (is_service_running("nut")) {
		log_error("Stopping service nut");
		stop_service("nut");
	}
 
	$nut_config = &$config['installedpackages']['nut']['config'][0];
	if (isset($nut_config['type'])) {
		$type = $nut_config['type'];
	} else {
		$type = 'disabled';
	}

	if ($type == 'disabled') {
		unlink_if_exists(NUT_RCFILE);
		unlink_if_exists(NUT_MONFILE);
		unlink_if_exists(NUT_UPSFILE);
		unlink_if_exists(NUT_UPSDFILE);
		unlink_if_exists(NUT_USERSFILE);
		return;
	}

	if ($type == 'remote_nut') {
		$user = $nut_config['remote_user'];
		$pass = $nut_config['remote_pass'];
		$monitor = $nut_config['name'] . "@" . $nut_config['remote_addr'];
		if (!empty($nut_config['remote_port'])) {
			$monitor .= ":" . $nut_config['remote_port'];
		}
		$monitor .= " 1 " . $nut_config['remote_user'] . " " . $nut_config['remote_pass'] . " slave";
	} else {
		if (!empty($nut_config['ups_conf'])) {
			$upsfile = base64_decode($nut_config['ups_conf']) . "\n\n";
		} else {
			$upsfile = "";
		}
		$upsfile .= "[" . $nut_config['name'] . "]\n";

		switch ($type) {
			case 'local_usb':
				$driver = $nut_config['usb_driver'];
				$port = "auto";
				break;
	
			case 'local_serial':
				$driver = $nut_config['serial_driver'];
				$port = $nut_config['serial_port'];
				break;
	
			case 'local_generic':
				$driver = "genericups";
				$port = $nut_config['serial_port'];
				$args = "upstype=" . $nut_config['generic_type'];
				break;
	
			case 'remote_apcupsd':
				$driver = "apcupsd-ups";
				$port = $nut_config['remote_addr'];
				if (!empty($nut_config['remote_port'])) {
					$port .= ":" . $nut_config['remote_port'];
				}
				break;
	
			case 'remote_netxml':
				$driver = "netxml-ups";
				$port = $nut_config['remote_proto'] . "://" .  $nut_config['remote_addr'];
				if (!empty($nut_config['remote_port'])) {
					$port .= ":" . $nut_config['remote_port'];
				}

				if (!empty($nut_config['remote_user'])) {
					$args = "login=\"" . $nut_config['remote_user'] . "\"";
					if (!empty($nut_config['remote_pass'])) {
						$args .= "\npassword=\"" . $nut_config['remote_pass'] . "\"";
					}
				}
				break;
	
			case 'remote_snmp':
				$driver = "snmp-ups";
				$port = $nut_config['remote_addr'];
				break;
		}

		$upsfile .= "driver=" . $driver . "\n";
		$upsfile .= "port=" . $port . "\n";
		if (!empty($args)) {
			$upsfile .= $args . "\n";
		}
		if (!empty($nut_config['extra_args'])) {
			$upsfile .= base64_decode($nut_config['extra_args']) . "\n";
		}
	  
		$upsdfile = "LISTEN 127.0.0.1\n";
		$upsdfile .= "LISTEN ::1\n";
		if (!empty($nut_config['upsd_conf'])) {
			$upsdfile .= "\n\n" . base64_decode($nut_config['upsd_conf']) . "\n";
		}

		$user = "monuser";
		$pass = bin2hex(openssl_random_pseudo_bytes(10));
		$monitor = $nut_config['name'] . " 1 " . $user . " " . $pass . " master";

		/* admin user to allow local ups administration */
		$usersfile = "[admin]\n";
		$usersfile .= "password=" . bin2hex(openssl_random_pseudo_bytes(10)) . "\n";
		$usersfile .= "actions=set\n";
		$usersfile .= "instcmds=all\n";
		$usersfile .= "[" . $user . "]\n";
		$usersfile .= "password=" . $pass . "\n";
		$usersfile .= "upsmon master\n";
		if (!empty($nut_config['upsd_users'])) {
			$usersfile .= "\n\n" . base64_decode($nut_config['upsd_users']) . "\n";
		}
	}


	$monfile = "MONITOR " . $monitor . "\n";
	$monfile .= "SHUTDOWNCMD \"/sbin/shutdown -p +0\"\n";
	$monfile .= "POWERDOWNFLAG /etc/killpower\n";

	if ($nut_config['email'] == 'yes') {
		$monfile .= "NOTIFYCMD /usr/local/pkg/nut/nut_email.php\n";
		$monfile .= "NOTIFYFLAG ONLINE   SYSLOG+WALL+EXEC\n";
		$monfile .= "NOTIFYFLAG ONBATT   SYSLOG+WALL+EXEC\n";
		$monfile .= "NOTIFYFLAG LOWBATT  SYSLOG+WALL+EXEC\n";
		$monfile .= "NOTIFYFLAG FSD	  SYSLOG+WALL+EXEC\n";
		$monfile .= "NOTIFYFLAG COMMOK   SYSLOG+WALL+EXEC\n";
		$monfile .= "NOTIFYFLAG COMMBAD  SYSLOG+WALL+EXEC\n";
		$monfile .= "NOTIFYFLAG SHUTDOWN SYSLOG+WALL+EXEC\n";
		$monfile .= "NOTIFYFLAG REPLBATT SYSLOG+WALL+EXEC\n";
		$monfile .= "NOTIFYFLAG NOCOMM   SYSLOG+WALL+EXEC\n";
		$monfile .= "NOTIFYFLAG NOPARENT SYSLOG+WALL+EXEC\n";
	}

	if (!empty($nut_config['upsmon_conf'])) {
		$monfile .= "\n\n" . base64_decode($nut_config['upsmon_conf']) . "\n";
	}

	conf_mount_rw();
	if (!empty($upsfile)) {
		nut_write_file(NUT_UPSFILE, $upsfile);
	} else {
		unlink_if_exists(NUT_UPSFILE);
	}
	if (!empty($upsdfile)) {
		nut_write_file(NUT_UPSDFILE, $upsdfile);
	} else {
		unlink_if_exists(NUT_UPSDFILE);
	}
	if (!empty($usersfile)) {
		nut_write_file(NUT_USERSFILE, $usersfile);
	} else {
		unlink_if_exists(NUT_USERSFILE);
	}
	nut_write_file(NUT_MONFILE, $monfile);
	nut_write_rcfile($driver);
	conf_mount_ro();

	if (platform_booting()) {
		return;
	}

	log_error("Starting service nut");
	start_service("nut");
}


function nut_ups_status()
{
	global $config;
	$status = array();

	$nut_config = &$config['installedpackages']['nut']['config'][0];
	if (isset($nut_config['type'])) {
		$type = $nut_config['type'];
	} else {
		$type = 'disabled';
	}

	if ($type == 'disabled') {
		$status['_summary'] = "Monitoring is not enabled";
		return $status;
	}

	$ups = $nut_config['name'];
	if ($type == 'remote_nut') {
		 $addr = $nut_config['remote_addr'];
		if (is_ipaddrv6($addr)) {
			/* upsc unfortunately requires brackes for ipv6 addresses */
			$ups .= "@[" . $addr . "]";
		} else {
			$ups .= "@" . $addr;
		}
		if (!empty($nut_config['remote_port'])) {
			$ups .= ":" . $nut_config['remote_port'];
		}
	} else {
		$ups .= "@" . "localhost";
	}
	$status['_name'] = $ups;

	/* Even though upsc might actually work (no password) it's better to report failure of upsmon */
	if (!is_process_running("upsmon")) {
		$status['_summary'] = "UPS Monitor not running";
		$status['_alert'] = true;
		return $status;
	}

	/* FIXME: use the -t option for upsc when it becomes available (targeted for nut 2.8.0) */
	$pipe = popen("/usr/local/bin/upsc $ups", 'r');
	if ($pipe) {
		while ($line = fgets($pipe)) {
			$index = strpos($line, ':');
			if ($index !== false) {
				$key = trim(substr($line, 0, $index));
				$val = trim(substr($line, $index + 1));
				$status[$key] = $val;
			}
		}
		pclose($pipe);
	}

	if (count($status) < 2) {
		$status['_summary'] = "Failed to retrieve status";
		$status['_alert'] = true;
		return $status;
	}

	if (isset($status['ups.status'])) {
		$str = strtok($status['ups.status'], ' ');
		while ($str) {
			switch ($str) {
				case 'OL':
					$summary .= "On line";
					break;
				case 'OB':
					$summary .= "On battery";
					$alert = true;
					break;
				case 'LB':
					$summary .= "Low battery";
					$alert = true;
					break;
				case 'HB':
					$summary .= "High battery";
					break;
				case 'RB':
					$summary .= "Replace battery";
					$alert = true;
					break;
				case 'CHRG':
					$summary .= "Charging";
					break;
				case 'DISCHRG':
					$summary .= "Discharging";
					break;
				case 'BYPASS':
					$summary .= "Bypass";
					$alert = true;
					break;
				case 'CAL':
					$summary .= "Calibrating";
					break;
				case 'OVER':
					$summary .= "Overloaded";
					$alert = true;
					break;
				case 'TRIM':
					$summary .= "Voltage trim";
					break;
				case 'BOOST':
					$summary .= "Voltage boost";
					break;
				default:
					$summary .= $str;
			}

			$str = strtok(' ');
			if ($str) {
				$summary .= ", ";
			}
		}

		$status['_summary'] = $summary;
	}

	if ($alert || isset($status['ups.alarm'])) {
		$status['_alert'] = true;
	}

	if (isset($status['battery.runtime'])) {
		$t = (int) $status['battery.runtime'];

		$s = $t % 60;
		$t /= 60;
		$m = $t % 60;
		$h = $t / 60;

		$status['_hms'] = sprintf("%d:%02d:%02d", $h, $m, $s);
	}

	return $status;
}


function nut_deinstall_command() {
	if (is_service_running("nut")) {
		log_error("Stopping service nut");
		stop_service("nut");
	}

	unlink_if_exists(NUT_RCFILE);
	unlink_if_exists(NUT_MONFILE);
	unlink_if_exists(NUT_UPSFILE);
	unlink_if_exists(NUT_UPSDFILE);
	unlink_if_exists(NUT_USERSFILE);
}
