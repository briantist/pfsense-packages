<?php
/* $Id$ */
/*
    autoconfigbackup.inc
    Copyright (C) 2008 Scott Ullrich

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

$pfSversion = str_replace("\n", "", file_get_contents("/etc/version"));
if(strstr($pfSversion, "1.2")) 
	require_once("crypt_acb.php");

/* ensures patches match */
function custom_php_validation_command($post, $input_errors) {
	global $_POST, $savemsg, $config;

	if($post['password'] <> $post['passwordagain']) 
		$input_errors[] = "Sorry, the entered passwords do not match.";

	if($post['crypto_password'] <> $post['crypto_password2']) 
		$input_errors[] = "Sorry, the entered encryption passwords do not match.";

	if($post['testconnection']) {
		$status = test_connection($post);
		if($status) 
			$savemsg = "Connection to portal.pfsense.org was tested with no errors.";
	}
	
	// We do not need to store this value.
	unset($_POST['testconnection']);
}

function test_connection($post) {
	global $savemsg, $config;

	// Seperator used during client / server communications
	$oper_sep			= "\|\|";

	// Encryption password
	$decrypt_password 	= $post['crypto_password'];

	// Defined username
	$username			= $post['username'];

	// Defined password
	$password			= $post['password'];

	// Set hostname
	$hostname			= $config['system']['hostname'] . "." . $config['system']['domain'];

	// URL to restore.php
	$get_url			= "https://{$username}:{$password}@portal.pfsense.org/pfSconfigbackups/restore.php";

	// Populate available backups
	$curl_session = curl_init();
	curl_setopt($curl_session, CURLOPT_URL, $get_url);
	curl_setopt($curl_session, CURLOPT_SSL_VERIFYPEER, 0);
	curl_setopt($curl_session, CURLOPT_POST, 1);
	curl_setopt($curl_session, CURLOPT_RETURNTRANSFER, 1);
	curl_setopt($curl_session, CURLOPT_POSTFIELDS, "action=showbackups&hostname={$hostname}");
	$data = curl_exec($curl_session);

	if (curl_errno($curl_session)) 
		return("An error occurred " . curl_error($curl_session));
	else 
		curl_close($curl_session);

	return;
}

function upload_config($reasonm = "") {
	global $config, $g, $input_errors;

	/*
	 *   pfSense upload config to pfSense.org script
	 *   This file plugs into filter.inc (/usr/local/pkg/pf)
	 *   and runs every time the running firewall filter changes.
	 * 
	 *   Written by Scott Ullrich
	 *   (C) 2008 BSD Perimeter LLC 
	 *
	 */
	
	if(file_exists("/tmp/acb_nooverwrite")) {
		unlink("/tmp/acb_nooverwrite");
		$nooverwrite = "true";
	} else {
		$nooverwrite = "false";
	}

	// Define some needed variables
	if(!file_exists("/cf/conf/lastpfSbackup.txt")) {
		conf_mount_rw();
		touch("/cf/conf/lastpfSbackup.txt");
		conf_mount_ro();
	}

	$last_backup_date 	= str_replace("\n", "", file_get_contents("/cf/conf/lastpfSbackup.txt"));
	$last_config_change = $config['revision']['time'];
	$hostname  			= $config['system']['hostname'] . "." . $config['system']['domain'];
	if($reasonm)
		$reason = $reasonm;
	else 
		$reason	   			= $config['revision']['description'];
	$username 			= $config['installedpackages']['autoconfigbackup']['config'][0]['username'];
	$password  			= $config['installedpackages']['autoconfigbackup']['config'][0]['password'];
	$encryptpw 			= $config['installedpackages']['autoconfigbackup']['config'][0]['crypto_password'];

	// Define upload_url, must be present after other variable definitions due to username, password
	$upload_url = "https://{$username}:{$password}@portal.pfsense.org/pfSconfigbackups/backup.php";

	if(!$username or !$password or !$encryptpw) {
		if(!file_exists("/cf/conf/autoconfigback.notice")) {
			$notice_text =  "Either the username, password or encryption password is not set for Automatic Configuration Backup.  ";
			$notice_text .= "Please correct this in Diagnostics -> AutoConfigBackup -> Settings.";
			log_error($notice_text);
			file_notice("AutoConfigBackup", $notice_text, $notice_text, "");
			conf_mount_rw();
			touch("/cf/conf/autoconfigback.notice");
			conf_mount_ro();
		}
	} else {
		/* If configuration has changed, upload to pfS */
		if($last_backup_date <> $last_config_change) {

				// Mount RW (if needed)
				conf_mount_rw();
				// Lock config
				config_lock();

				$notice_text = "Beginning https://portal.pfsense.org configuration backup.";
				log_error($notice_text);
				update_filter_reload_status($notice_text);

				// Encrypt config.xml
				$raw_config_sha256_hash = trim(`/sbin/sha256 /cf/conf/config.xml | awk '{ print $4 }'`);
				$data = file_get_contents("/cf/conf/config.xml");
				$data = encrypt_data($data, $encryptpw);
				tagfile_reformat($data, $data, "config.xml");

				$post_fields = array(
					'reason'					=> urlencode($reason),  
					'hostname'					=> urlencode($hostname),  
					'configxml'					=> urlencode($data),
					'nooverwrite'				=> urlencode($nooverwrite),
					'raw_config_sha256_hash' 	=> urlencode($raw_config_sha256_hash)
				);

				//url-ify the data for the POST  
				foreach($post_fields as $key=>$value) 
					$fields_string .= $key.'='.$value.'&'; 
				rtrim($fields_string,'&');

				// Check configuration into the BSDP repo
				$curl_session = curl_init();
				curl_setopt($curl_session, CURLOPT_URL, $upload_url);  
				curl_setopt($curl_session, CURLOPT_POST, count($post_fields));  
				curl_setopt($curl_session, CURLOPT_POSTFIELDS, $fields_string);
				curl_setopt($curl_session, CURLOPT_RETURNTRANSFER, 1);		
				curl_setopt($curl_session, CURLOPT_SSL_VERIFYPEER, 0);
				$data = curl_exec($curl_session);
				if (curl_errno($curl_session)) {
					$fd = fopen("/tmp/backupdebug.txt", "w");
					fwrite($fd, $upload_url . "" . $fields_string . "\n\n");
					fwrite($fd, $data);
					fwrite($fd, curl_error($curl_session));
					fclose($fd);
				} else {
				    curl_close($curl_session);
				}

				if(!strstr($data, "500")) {
					$notice_text = "An error occured while uploading your pfSense configuration to portal.pfsense.org";
					log_error($notice_text . " - " . $data);
					file_notice("autoconfigurationbackup", $notice_text, $data, "");			
					update_filter_reload_status($notice_text . " - " . $data);	
				} else {
					// Update last pfS backup time
					$fd = fopen("/cf/conf/lastpfSbackup.txt", "w");
					fwrite($fd, $config['revision']['time']);
					fclose($fd);				
					$notice_text = "End of portal.pfsense.org configuration backup (success).";
					log_error($notice_text);
					update_filter_reload_status($notice_text);	
				}

				// Unlock config
				config_unlock();
				// Mount image RO (if needed)
				conf_mount_ro();

		} else {
			log_error("No https://portal.pfsense.org backup required.");
		}

	}	
}

?>