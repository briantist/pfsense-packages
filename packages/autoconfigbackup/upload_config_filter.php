<?php

/*
 *   pfSense upload config to pfSense.org script
 *   This file plugs into filter.inc (/usr/local/pkg/pf)
 *   and runs every time the running firewall filter changes.
 * 
 *   Written by Scott Ullrich
 *   (C) 2008 BSD Perimeter LLC 
 *
 */

$last_backup_date 	= $config['system']['lastpfSbackup'];
$last_config_change = $config['revision']['time'];
$hostname  			= $config['system']['hostname'];
$username 			= $config['installedpackages']['autoconfigbackup']['config'][0]['username'];
$password  			= $config['installedpackages']['autoconfigbackup']['config'][0]['password'];
$encryptpw 			= $config['installedpackages']['autoconfigbackup']['config'][0]['crypto_password'];
$reason	   			= $config['revision']['description'];

/* If configuration has changed, upload to pfS */
if($last_backup_date <> $last_config_change) {

	if($username && $password && $encryptpw) {

		log_error("Beginning portal.pfsense.org configuration backup.");

		$upload_url = "https://{$username}:{$password}@portal.pfsense.org/pfSconfigbackups/backup.php";

		// Encrypt config.xml
		$data = file_get_contents("/cf/conf/config.xml");
		$configxml = encrypt_data($data, $encryptpw);
		tagfile_reformat($data, $data, "config.xml");

		// Check configuration into the BSDP repo
		$curl_Session = curl_init($upload_url);
		curl_setopt($curl_Session, CURLOPT_POST, 1);
		curl_setopt($curl_Session, CURLOPT_POSTFIELDS, "reason={$reason}&configxml={$configxml}&hostname={$hostname}");
		curl_setopt($curl_Session, CURLOPT_FOLLOWLOCATION, 1);
		$data = curl_exec($curl_Session);
		curl_close($curl_Session);

		$config['system']['lastpfSbackup'] = $last_config_change;
		write_config("Updating last portal.pfsense.org last backup date/time.");

		log_error("End of portal.pfsense.org configuration backup.");

	}
}

?>