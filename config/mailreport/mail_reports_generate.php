#!/usr/local/bin/php -q
<?php
/* $Id$ */
/*
	mail_reports.inc
	Part of pfSense
	Copyright (C) 2011 Jim Pingle <jimp@pfsense.org>
	Portions Copyright (C) 2007-2011 Seth Mos <seth.mos@dds.nl>
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

require_once("mail_reports.inc");
require_once("notices.inc");

$id = $_GET['id'];
if (isset($_POST['id']))
	$id = $_POST['id'];

if (!isset($id) && isset($argv[1]))
	$id = $argv[1];

// No data, no report to run, bail!
if (!isset($id))
	exit;

// No reports to run, bail!
if (!is_array($config['mailreports']['schedule']))
	exit;

// The Requested report doesn't exist, bail!
if (!$config['mailreports']['schedule'][$id])
	exit;

$thisreport = $config['mailreports']['schedule'][$id];
$graphs = $thisreport['row'];

// No graphs on the report, bail!
if (!is_array($graphs) || !(count($graphs) > 0))
	exit;

// Print report header

// For each graph, print a header and the graph
$attach = array();
$idx=0;
foreach ($graphs as $thisgraph) {
	$dates = get_dates($thisgraph['period'], $thisgraph['timespan']);
	$start = $dates['start'];
	$end = $dates['end'];
	$attach[] = mail_report_generate_graph($thisgraph['graph'], $thisgraph['style'], $thisgraph['timespan'], $start, $end);
}

mail_report_send($thisreport['descr'], $attach);

?>