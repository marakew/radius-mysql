#! /usr/bin/perl
#
# checkrad.pl	See if a user is (still) logged in on a certain port.
#
#		This is used by the cistron-radius server to check
#		if its idea of a user logged in on a certain port/nas
#		is correct if a double login is detected.
#
# Called as:	nas_type nas_ip nas_port login session_id
#
# Returns:	0 = no duplicate, 1 = duplicate, >1 = error.
#
# Version:	@(#)checkrad.pl  1.31  22-Mar-2001  miquels@cistron.nl
#
#		livingston_snmp  1.2	Author: miquels@cistron.nl
#		cvx_snmp	 1.0	Author: miquels@cistron.nl
#		portslave_finger 1.0	Author: miquels@cistron.nl
#		max40xx_finger	 1.0	Author: costa@mdi.ca
#		ascend_snmp      1.1    Author: blaz@amis.net
#		computone_finger 1.2	Author: pacman-radius@cqc.com
#		sub tc_tccheck	 1.1	Author: alexisv@compass.com.ph
#		cyclades_telnet	 1.2	Author: accdias@sst.com.br
#		patton_snmp	 1.0	Author: accdias@sst.com.br
#		digitro_rusers	 1.1	Author: accdias@sst.com.br
#		cyclades_snmp	 1.1	Author: accdias@sst.com.br
#		usrhiper_snmp    1.0    Author: igor@ipass.net
#		multitech_snmp   1.0    Author: ehonzay@willmar.com
#		netserver_telnet 1.0	Author: mts@interplanet.es
#		versanet_snmp    1.0    Author: support@versanetcomm.com
#		bay_finger	 1.0	Author: chris@shenton.org
#
#	Config: $debug is the file you want to put debug messages in
#		$snmpget is the location of your ``snmpget'' program
#		$snmpwalk is the location of your ``snmpwalk'' program
#		$rusers is the location of your ``rusers'' program
#		$naspass is the location of your NAS admin password file
#

$prefix		= "/usr/local";
$localstatedir	= "/var";
$logdir		= "${localstatedir}/log";
$sysconfdir	= "/etc";
$raddbdir	= "${sysconfdir}/raddb";

$debug		= "";
#$debug		= "$logdir/checkrad.log";

$snmpget	= "/usr/bin/snmpget";
$snmpwalk	= "/usr/bin/snmpwalk";
$rusers		= "/usr/bin/rusers";
$naspass	= "$raddbdir/naspasswd";

#
#	USR-Hiper: $hiper_density is the reported port density (default 256
#	but 24 makes more sense)
#
$hiper_density = 256;

#
#	Try to load Net::Telnet, SNMP_Session etc.
#	Do not complain if we cannot find it.
#	Prefer a locally installed copy.
#
BEGIN {
	unshift @INC, "/usr/local/lib/site_perl";

	eval "use Net::Telnet 3.00;";
	$::HAVE_NET_TELNET = ($@ eq "");

	eval "use SNMP_Session;";
	if ($@ eq "") {
		eval "use BER;";
		$::HAVE_SNMP_SESSION = ($@ eq "");
		eval "use Socket;";
	}
};

#
#	Get password from /etc/raddb/naspasswd file.
#	Returns (login, password).
#
sub naspasswd {
	my ($terminalserver, $emptyok) = @_;
	my ($login, $password);
	my ($ts, $log, $pass);

	unless (open(NFD, $naspass)) {
		if (!$emptyok) {
			print LOG "checkrad: naspasswd file not found; " .
			"possible match for $ARGV[3]\n" if ($debug);
			print STDERR "checkrad: naspasswd file not found; " .
			"possible match for $ARGV[3]\n";
		}
		return ();
	}
	while (<NFD>) {
		chop;
		next if (m/^(#|$|[\t ]+$)/);
		($ts, $log, $pass) = split(/\s+/, $_, 3);
		if ($ts eq $terminalserver) {
			$login = $log;
			$password = $pass;
			last;
		}
	}
	close NFD;
	if ($password eq "" && !$emptyok) {
		print LOG "checkrad: password for $ARGV[1] is null; " .
			"possible match for $ARGV[3] on " . 
			"port $ARGV[2]\n" if ($debug);
		print STDERR "checkrad: password for $ARGV[1] is null; " .
			"possible match for $ARGV[3] on port $ARGV[2]\n"; 
	}
	($login, $password);
}

#
#	See if Net::Telnet is there.
#
sub check_net_telnet {
	if (!$::HAVE_NET_TELNET) {
		print LOG
		"  checkrad: Net::Telnet 3.00+ CPAN module not installed\n"
		if ($debug);
		print STDERR
		"checkrad: Net::Telnet 3.00+ CPAN module not installed\n";
		return 0;
	}
	1;
}

#
#	Do snmpget by calling snmpget.
#
sub snmpget_prog {
	my ($host, $community, $oid) = @_;
	my ($ret);
	local $_;

	$_ = `$snmpget $host $community $oid`;
	if (/^.*(\s|\")([0-9A-Za-z]{8})(\s|\"|$).*$/) {
		# Session ID format.
		$ret = $2;
	} elsif (/^.*=.*"(.*)"/) {
		# oid = "...." junk format.
		$ret = $1;
	} elsif (/^.*=\s*(\S+)/) {
		# oid = string format
		$ret = $1;
	}

	# Strip trailing junk if any.
	$ret =~ s/\s*Hex:.*$//;
	$ret;
}

#
#	Do snmpget by using SNMP_Session.
#	Coded by Jerry Workman <jerry@newwave.net>
#
sub snmpget_session {
	my ($host, $community, $OID) = @_;
	my ($ret);
	local $_;
	my (@enoid, $var,$response, $bindings, $binding, $value);
	my ($inoid, $outoid, $upoid, $oid, @retvals);

	$OID =~ s/^.iso.org.dod.internet.private.enterprises/.1.3.6.1.4.1/;

	push @enoid,  encode_oid((split /\./, $OID));
	srand();

	my $session = SNMP_Session->open($host, $community, 161);
	if (!$session->get_request_response(@enoid)) {
		$e = "No SNMP answer from $ARGV[0].";
		print LOG "$e\n" if ($debug);
		print STDERR "checkrad: $e\n";
		return "";
	}
	$response = $session->pdu_buffer;
	($bindings) = $session->decode_get_response ($response);
	$session->close ();
	while ($bindings) {
		($binding,$bindings) = decode_sequence ($bindings);
		($oid,$value) = decode_by_template ($binding, "%O%@");
		my $tempo = pretty_print($value);
		$tempo=~s/\t/ /g;
		$tempo=~s/\n/ /g;
		$tempo=~s/^\s+//;
		$tempo=~s/\s+$//;

		push @retvals, $tempo;
	}
	$retvals[0];
}

#
#	Do snmpget
#
sub snmpget {
	my $ret;

	if ($::HAVE_SNMP_SESSION) {
		$ret = snmpget_session(@_);
	} elsif (-x $snmpget) {
		$ret = snmpget_prog(@_);
	} else {
		$e = "Neither SNMP_Session module or $snmpget found!";
		print LOG "$e\n" if ($debug);
		print STDERR "checkrad: $e\n";
		$ret = "";
	}
	$ret;
}

#
#	Strip domains, prefixes and suffixes from username
#	
#	Known prefixes: (P)PP, (S)LIP e (C)SLIP
#	Known suffixes: .ppp, .slip e .cslip
#
#	Author: Antonio Dias of SST Internet <accdias@sst.com.br>
#
sub strip_username {
	my ($user) = @_;
	#
	#	Trim white spaces.
	#
	$user =~ s/^\s*(.*?)\s*$/$1/;
	#
	#	Strip out domains, prefix and suffixes
	#
	$user =~ s/\@(.)*$//;
	$user =~ s/^[PSC]//;
	$user =~ s/\.(ppp|slip|cslip)$//;
	$user;
}

#
#	See if the user is logged in using the Livingston MIB.
#	We don't check the username but the session ID.
#
$lvm	 = '.iso.org.dod.internet.private.enterprises.307';
sub livingston_snmp {
	#
	#	We don't know at which ifIndex S0 is, and
	#	there might be a hole at S23, or at S30+S31.
	#	So we figure out dynamically which offset to use.
	#
	#	If the port < S23, probe ifIndex 5.
	#	If the port < S30, probe IfIndex 23.
	#	Otherwise probe ifIndex 32.
	#
	my $ifIndex;
	my $test_index;
	if ($ARGV[2] < 23) {
		$test_index = 5;
	} elsif ($ARGV[2] < 30) {
		$test_index = 23;
	} else {
		$test_index = 32;
	}
	$_ = snmpget($ARGV[1], "public", "$lvm.3.2.1.1.1.2.$test_index");
	/S([0-9]+)/;
	$xport = $1 + 0;
	$ifIndex = $ARGV[2] + ($test_index - $xport);

	print LOG "  port S$ARGV[2] at SNMP ifIndex $ifIndex\n"
		if ($debug);

	#
	#	Now get the session id from the terminal server.
	#
	$sessid = snmpget($ARGV[1], "public", "$lvm.3.2.1.1.1.5.$ifIndex");

	print LOG "  session id at port S$ARGV[2]: $sessid\n" if ($debug);

	($sessid eq $ARGV[4]) ? 1 : 0;
}

#
#	See if the user is logged in using the Aptis MIB.
#	We don't check the username but the session ID.
#
# sessionStatusActiveName
$apm1	 = '.iso.org.dod.internet.private.enterprises.2637.2.2.102.1.12';
# sessionStatusActiveStopTime
$apm2	 = '.iso.org.dod.internet.private.enterprises.2637.2.2.102.1.20';
sub cvx_snmp {

	# Remove unique identifier, then take remainder of the
	# session-id as a hex number, convert that to decimal.
	my $sessid = $ARGV[4];
	$sessid =~ s/^.*://;
	$sessid =~ s/^0*//;
	$sessid = "0" if ($sessid eq '');

	#
	#	Now get the login from the terminal server.
	#	Blech - the SNMP table is called 'sessionStatusActiveTable,
	#	but it sometimes lists inactive sessions too.
	#	However an active session doesn't have a Stop time,
	#	so we can differentiate that way.
	#
	my $login = snmpget($ARGV[1], "public", "$apm1." . hex($sessid));
	my $stopt = snmpget($ARGV[1], "public", "$apm2." . hex($sessid));
	$login = "--" if ($stopt > 0);

	print LOG "  login with session-id $ARGV[4]: $login\n" if ($debug);

	(strip_username($login) eq strip_username($ARGV[3])) ? 1 : 0;
}

#
#	See if the user is logged in using the Cisco MIB
#
$csm	 = '.iso.org.dod.internet.private.enterprises.9';
sub cisco_snmp {

	# Look up community string in naspasswd file.
	my ($login, $pass) = naspasswd($ARGV[1], 1);
	if ($login && $login ne 'SNMP') {
		if ($debug) {
			print LOG
			"   Error: Need SNMP community string for $ARGV[1]\n";
		}
		return 2;
	} else {
		$pass = "public";
	}

	my $port = $ARGV[2];
	my $sess_id = hex($ARGV[4]);

	if ($port < 20000) {
		#
		#	The AS5350 doesn't support polling the session ID,
		#	so we do it based on nas-port-id. This only works
		#	for analog sessions where port < 20000.
		#	Yes, this means that simultaneous-use on the as5350
		#	doesn't work for ISDN users.
		#
		$login = snmpget($ARGV[1], $pass, "$csm.2.9.2.1.18.$port");
		print LOG "  user at port S$port: $login\n" if ($debug);
	} else {
		$login = snmpget($ARGV[1], $pass,
				"$csm.9.150.1.1.3.1.2.$sess_id");
		print LOG "  user with session id $ARGV[4] ($sess_id): " .
			"$login\n" if ($debug);
	}

	($login eq $ARGV[3]) ? 1 : 0;
}

#
#       Check a MultiTech CommPlete Server ( CC9600 & CC2400 )
#
#       Author: Eric Honzay of Bennett Office Products <ehonzay@willmar.com>
#
$msm    = '.iso.org.dod.internet.private.enterprises.995';
sub multitech_snmp {
	my $temp = $ARGV[2] + 1;

        $login = snmpget($ARGV[1], "public", "$msm.2.31.1.1.1.$temp");
        print LOG " user at port S$ARGV[2]: $login\n" if ($debug);

        ($login eq $ARGV[3]) ? 1 : 0;
}

#
#       Check a Computone Powerrack via finger
#
#       Old Author: Shiloh Costa of MDI Internet Inc. <costa@mdi.ca>
#       New Author: Alan Curry of ComQuest Internet Services. <pacman@cqc.com>
#
# The finger response format is version-dependent. To do this *right*, you
# need to know exactly where the port number and username are. I know that
# for 1.7.2, and 3.0.4 but for others I just guess.
# Oh yeah and on top of it all, the thing truncates usernames. --Pac.
#
# 1.7.2 and 3.0.4 both look like this:
#
# 0    0 000 00:56 luser         pppfsm  Incoming PPP, ppp00, 10.0.0.1
#
# and the truncated ones look like this:
#
# 25   0 000 00:15 longnameluse..pppfsm  Incoming PPP, ppp25, 10.0.0.26
#
# Yes, the fields run together. Long Usernames Considered Harmful.
#
sub computone_finger {
	my $trunc, $ver;

	open(FD, "finger \@$ARGV[1]|") or return 2;
	<FD>; # the [hostname] line is definitely uninteresting
	$trunc = substr($ARGV[3], 0, 12);
	$ver = "";
	while(<FD>) {
		if(/cnx kernel release ([^ ,]+)[, ]/) {
			$ver = $1;
			next;
		}
		# Check for known versions
		if ($ver eq '1.7.2' || $ver eq '3.0.4') {
			if (/^\Q$ARGV[2]\E\s+\S+\s+\S+\s+\S+\s+\Q$trunc\E(\s+|\.\.)/) {
	 			close FD;
				return 1;
			}
			next;
		}
		# All others.
		if (/^\s*\Q$ARGV[2]\E\s+.*\s+\Q$trunc\E\s+/) {
			close FD;
			return 1;
		}
	}

	close FD;
	return 0;
}

#
#	Check an Ascend Max4000 or similar model via finger
#
#	Note: Not all software revisions support finger
#	      You may also need to enable the finger option.
#
#	Author: Shiloh Costa of MDI Internet Inc. <costa@mdi.ca>
#
sub max40xx_finger {
	open(FD, "finger $ARGV[3]\@$ARGV[1]|");
	while(<FD>) {
	   $line = <FD>;
	   if( $line =~ /Session/ ){
	      return 1; # user is online
	   }else{
	      return 0; # user is offline
	   }
	}
	close FD;
}


#
#	Check an Ascend Max4000 or similar model via SNMP
#
#	Author: Blaz Zupan of Medinet <blaz@amis.net>
#
$asm   = '.iso.org.dod.internet.private.enterprises.529';
sub ascend_snmp {
	my $sess_id;
	my $l1, $l2;

	$l1 = '';
	$l2 = '';

	#
	#	If it looks like hex, only try it as hex,
	#	otherwise try it as both decimal and hex.
	#
	$sess_id = $ARGV[4];
	if ($sess_id !~ /^0/ && $sess_id !~ /[a-f]/i) {
		$l1 = snmpget($ARGV[1], "public", "$asm.12.3.1.4.$sess_id");
	}
	$sess_id = hex $ARGV[4];
	$l2 = snmpget($ARGV[1], "public", "$asm.12.3.1.4.$sess_id");

	print LOG "  user at port S$ARGV[2]: $l1 (dec)\n" if ($debug && $l1);
	print LOG "  user at port S$ARGV[2]: $l2 (hex)\n" if ($debug && $l2);

	(($l1 && $l1 eq $ARGV[3]) || ($l2 && $l2 eq $ARGV[3])) ? 1 : 0;
}


#
#	See if the user is logged in using the portslave finger.
#
sub portslave_finger {
	my ($Port_seen);

	$Port_seen = 0;

	open(FD, "finger \@$ARGV[1]|");
	while(<FD>) {
		#
		#	Check for ^Port. If we don't see it we
		#	wont get confused by non-portslave-finger
		#	output too.
		#
		if (/^Port/) {
			$Port_seen++;
			next;
		}
		next if (!$Port_seen);
		next if (/^---/);

		($port, $user) = /^.(...) (...............)/;

		$port =~ s/ .*//;
		$user =~ s/ .*//;
		$ulen = length($user);
		#
		#	HACK: strip [PSC] from the front of the username,
		#	and things like .ppp from the end.
		#
		$user =~ s/^[PSC]//;
		$user =~ s/\.(ppp|slip|cslip)$//;

		#
		#	HACK: because ut_user usually has max. 8 characters
		#	we only compare up the the length of $user if the
		#	unstripped name had 8 chars.
		#
		$argv_user = $ARGV[3];
		if ($ulen == 8) {
			$ulen = length($user);
			$argv_user = substr($ARGV[3], 0, $ulen);
		}

		if ($port == $ARGV[2]) {
			if ($user eq $argv_user) {
				print LOG "  $user matches $argv_user " .
					"on port $port" if ($debug);
				close FD;
				return 1;
			} else {
				print LOG "  $user doesn't match $argv_user " .
					"on port $port" if ($debug);
				close FD;
				return 0;
			}
		}
	}
	close FD;
	0;
}

#
#	See if the user is already logged-in at the 3Com/USR Total Control.
#	(this routine by Alexis C. Villalon <alexisv@compass.com.ph>).
#	You must have the Net::Telnet module from CPAN for this to work.
#	You must also have your /etc/raddb/naspasswd made up.
# 
sub tc_tccheck {
	#
	#	Localize all variables first.
	#
	my ($Port_seen, $ts, $terminalserver, $log, $login, $pass, $password);
	my ($telnet, $curprompt, $curline, $ok, $totlines, $ccntr);
	my (@curlines, @cltok, $user, $port, $ulen);

	return 2 unless (check_net_telnet());

	$terminalserver = $ARGV[1];
	$Port_seen = 0;
	#
	#	Get login name and password for a certain NAS from $naspass.
	#
	($login, $password) = naspasswd($terminalserver, 1);
	return 2 if ($password eq "");

	#
	#	Communicate with NAS using Net::Telnet, then issue
	#	the command "show sessions" to see who are logged in.
	#	Thanks to Chris Jackson <chrisj@tidewater.net> for the
	#	for the "-- Press Return for More --" workaround.
	#
	$telnet = new Net::Telnet (Timeout => 10,
				   Prompt => '/\>/');
	$telnet->open($terminalserver);
	$telnet->login($login, $password);
	$telnet->print("show sessions");
	while ($curprompt ne "\>") {
		($curline, $curprompt) = $telnet->waitfor
			(String => "-- Press Return for More --",
			 String => "\>",
			 Timeout => 10);
		$ok = $telnet->print("");
		push @curlines, split(/^/m, $curline);
	}
	$telnet->close;
	#
	#	Telnet closed.  We got the info.  Let's examine it.
	#
	$totlines = @curlines;
	$ccntr = 0;
	while($ccntr < $totlines) {
		#
		#	Check for ^Port.
		#
		if ($curlines[$ccntr] =~ /^Port/) {
			$Port_seen++;
			$ccntr++;
			next;
		}
		#
		#	Ignore all unnecessary lines.
		#
		if (!$Port_seen || $curlines[$ccntr] =~ /^---/ ||
			$curlines[$ccntr] =~ /^ .*$/) {
			$ccntr++;
			next;
		}
		#
		#	Parse the current line for the port# and username.
		#
		@cltok = split(/\s+/, $curlines[$ccntr]);
		$ccntr++;
		$port = $cltok[0];
		$user = $cltok[1];
		$ulen = length($user);
		#
		#	HACK: strip [PSC] from the front of the username,
		#	and things like .ppp from the end.  Strip S from
		#	the front of the port number.
		#
		$user =~ s/^[PSC]//;
		$user =~ s/\.(ppp|slip|cslip)$//;
		$port =~ s/^S//;
		#		
		#	HACK: because "show sessions" shows max. 15 characters
		#	we only compare up to the length of $user if the
		#	unstripped name had 15 chars.
		#
		$argv_user = $ARGV[3];
		if ($ulen == 15) {
			$ulen = length($user);
			$argv_user = substr($ARGV[3], 0, $ulen);
		}
		if ($port == $ARGV[2]) {
			if ($user eq $argv_user) {
				print LOG "  $user matches $argv_user " .
					"on port $port" if ($debug);
				return 1;
			} else {
				print LOG "  $user doesn't match $argv_user " .
					"on port $port" if ($debug);
				return 0;
			}
		}
	}
	0;
}

#
#	Check a Cyclades PathRAS via telnet
#
#	Version: 1.2
#
#	Author: Antonio Dias of SST Internet <accdias@sst.com.br>
#
sub cyclades_telnet {
	#
	#	Localize all variables first.
	#
	my ($pr, $pr_login, $pr_passwd, $pr_prompt, $endlist, @list, $port, $user);
	#
	#	This variable must match PathRAS' command prompt 
	#	string as entered in menu option 6.2.
	#	The value below matches the default command prompt.
	#
	$pr_prompt = '/Select option ==\>$/i';

	#
	#	This variable match the end of userslist.
	#
	$endlist = '/Type \<enter\>/i';

	#
	#	Do we have Net::Telnet installed?
	#
	return 2 unless (check_net_telnet());

	#
	#	Get login name and password for NAS 
	#	from $naspass file.
	#
	($pr_login, $pr_passwd) = naspasswd($ARGV[1], 1);

	#
	#	Communicate with PathRAS using Net::Telnet, then access
	#	menu option 6.8 to see who are logged in.
	#	Based on PathRAS firmware version 1.2.3
	#
	$pr = new Net::Telnet (
		Timeout		=> 10,
		Host		=> $ARGV[1],
		ErrMode		=> 'return'
	) || return 2;

	#
	#	Force PathRAS shows its banner.
	#
	$pr->break();

	#
	#	Log on PathRAS
	#
	if ($pr->waitfor(Match => '/login : $/i') == 1) {
		$pr->print($pr_login);
	} else { 
		print LOG " Error: sending login name to PathRAS\n" if ($debug);
		$pr->close;
		return 2;	
	}

	if ($pr->waitfor(Match => '/password : $/i') == 1) {
		$pr->print($pr_passwd);
	} else { 
		print LOG " Error: sending password to PathRAS.\n" if ($debug);
		$pr->close;
		return 2;	
	}

	$pr->print();

	#
	#	Access menu option 6 "PathRAS Management"
	#
	if ($pr->waitfor(Match => $pr_prompt) == 1) {
		$pr->print('6');
	} else { 
		print LOG "  Error: acessing menu option '6'.\n" if ($debug);
		$pr->close;
		return 2;
	}
	#
	#	Access menu option 8 "Show Active Ports"
	#
	if ($pr->waitfor(Match => $pr_prompt) == 1) {
		@list = $pr->cmd(String => '8', Prompt => $endlist);
	} else { 
		print LOG "  Error: acessing menu option '8'.\n" if ($debug);
		$pr->close;
		return 2;
	}
	#
	#	Since we got the info we want, let's close 
	#	the telnet session
	#
	$pr->close;

	#
	#	Lets examine the userlist stored in @list
	#
	foreach(@list) {
		#
		#	We are interested in active sessions only
		#
		if (/Active/i) {
			($port, $user) = split;
			#
			#	Strip out any prefix, suffix and
			#	realm from $user check to see if
			#	$ARGV[3] matches.
			#
			if($ARGV[3] eq strip_username($user)) {
				print LOG "  User '$ARGV[3]' found on '$ARGV[1]:$port'.\n" if ($debug);
				return 1;
			}
		}
	}
	print LOG "  User '$ARGV[3]' not found on '$ARGV[1]'.\n" if ($debug);
   	0;
}

#
#	Check a Patton 2800 via SNMP
#
#	Version: 1.0
#
#	Author: Antonio Dias of SST Internet <accdias@sst.com.br>
#
sub patton_snmp {
   my($oid);

   #$oid = '.1.3.6.1.4.1.1768.5.100.1.40.' . hex $ARGV[4];
   # Reported by "Andria Legon" <andria@patton.com>
   # The OID below should be the correct one instead of the one above.
   $oid = '.1.3.6.1.4.1.1768.5.100.1.56.' . hex $ARGV[4];
   #
   # Check if the session still active
   # 
   if (snmpget($ARGV[1], "monitor", "$oid") == 0) {
      print LOG "  Session $ARGV[4] still active on NAS " . 
      	"$ARGV[1], port $ARGV[2], for user $ARGV[3].\n" if ($debug);
      return 1;
   }
   0;
}

#
#      Check a Digitro BXS via rusers
#
#      Version: 1.1
#
#      Author: Antonio Dias of SST Internet <accdias@sst.com.br>
#
sub digitro_rusers {
   my ($ret);
   local $_;

   if (-e $rusers && -x $rusers) {
      #
      # Get a list of users logged in via rusers
      #
      $_ = `$rusers $ARGV[1]`;
      $ret = ((/$ARGV[3]/) ? 1 : 0);
   } else {
      print LOG "   Error: can't execute $rusers\n" if $debug;
      $ret = 2;
   }
   $ret;
}

#
#	Check Cyclades PR3000 and PR4000 via SNMP
#
#	Version: 1.0
#
#	Author: Antonio Dias of SST Internet <accdias@sst.com.br>
#
sub cyclades_snmp {
   my ($oid, $ret);
   local $_;
   
   $oid = ".1.3.6.1.4.1.2925.3.3.6.1.1.2";

   if (-e $snmpwalk && -x $snmpwalk) { 	
      #
      # Get a list of logged in users via snmpwalk
      #
      # I'm looking for a way to do this using SNMP_Session
      # module. Any help would be great.
      #
      $_ = `$snmpwalk $ARGV[1] public $oid`;
      $ret = ((/"$ARGV[3]"/) ? 1 : 0);
   } else {
      print LOG "   Error: can't execute $snmpwalk\n" if $debug;
      $ret = 2;
   }
   $ret;
}

#
#	3Com/USR HiPer Arc Total Control.
#	This works with HiPer Arc 4.0.30
#	(this routine by Igor Brezac <igor@ipass.net>)
# 
$usrm	 = '.iso.org.dod.internet.private.enterprises.429';
sub usrhiper_snmp {
	my ($login,$password,$oidext);

	# Look up community string in naspasswd file.
        ($login, $password) = naspasswd($ARGV[1], 1);
	if ($login && $login ne 'SNMP') {
		if($debug) {
			print LOG
			"   Error: Need SNMP community string for $ARGV[1]\n";
		}
		return 2;
	} else {
		$password = "public";
	}

	$oidext = 1257 + 256*int(($ARGV[2]-1) / $hiper_density) +
		                (($ARGV[2]-1) % $hiper_density);
	my ($login);

	$login = snmpget($ARGV[1], $password, "$usrm.4.10.1.1.18.$oidext");
	if ($login =~ /\"/) {
		$login =~ /^.*\"([^"]+)\"/;
		$login = $1;
	}

	print LOG "  user at port S$ARGV[2]: $login\n" if ($debug);

	($login eq $ARGV[3]) ? 1 : 0;
}


#
#	Check USR Netserver with Telnet - based on tc_tccheck.
#	By "Marti" <mts@interplanet.es>
#
sub usrnet_telnet {
	#
	#	Localize all variables first.
	#
	my ($ts, $terminalserver, $login, $password);
	my ($telnet, $curprompt, $curline, $ok);
	my (@curlines, $user, $port);

	return 2 unless (check_net_telnet());

	$terminalserver = $ARGV[1];
	$Port_seen = 0;
	#
	#	Get login name and password for a certain NAS from $naspass.
	#
	($login, $password) = naspasswd($terminalserver, 1);
	return 2 if ($password eq "");

	#
	#	Communicate with Netserver using Net::Telnet, then access
	#	list connectionsto see who are logged in. 
	# 
	$telnet = new Net::Telnet (Timeout => 10,
				   Prompt => '/\>/');
	$telnet->open($terminalserver);

	#
	#	Log on Netserver
	#
	$telnet->login($login, $password);

	#
	#     Launch list connections command

	$telnet->print("list connections");

	while ($curprompt ne "\>") {
		($curline, $curprompt) = $telnet->waitfor
			( String => "\>",
			 Timeout => 10);
		$ok = $telnet->print("");
		push @curlines, split(/^/m, $curline);
	}

	$telnet->close;
	#
	#	Telnet closed.  We got the info.  Let's examine it.
	#
	foreach(@curlines) {
		if ( /mod\:/ ) {
			($port, $user, $dummy) = split;
			#
			# Strip out any prefixes and suffixes 
			# from the username
			#
			# uncomment this if you use the standard
			# prefixes
			#$user =~ s/^[PSC]//;
			#$user =~ s/\.(ppp|slip|cslip)$//;
			#
			# Check to see if $user is already connected
			#
			if ($user eq $ARGV[3]) {
				print LOG "  $user matches $ARGV[3] " .
					"on port $port" if ($debug);
				return 1;
			};
		};
	};
	print LOG 
	"  $ARGV[3] not found on Netserver logged users list " if ($debug);
	0;
}

#
#	Versanet's Perl Script Support:
#
#	___ versanet_snmp 1.0 by support@versanetcomm.com ___ July 1999
#	Versanet Enterprise MIB Base: 1.3.6.1.4.1.2180
#   
#	VN2001/2002 use slot/port number to locate modems. To use snmp get we
#	have to translate the original port number into a slot/port pair.
#
$vsm     = '.iso.org.dod.internet.private.enterprises.2180';
sub versanet_snmp {
        
	print LOG "argv[2] = $ARGV[2] " if ($debug);
	$port = $ARGV[2]%8;
	$port = 8 if ($port eq 0);	  
	print LOG "port = $port " if ($debug);
	$slot = (($ARGV[2]-$port)/8)+1;
	print LOG "slot = $slot" if ($debug);
	$loginname = snmpget($ARGV[1], "public", "$vsm.27.1.1.3.$slot.$port");
#
#	Note: the "public" string above could be replaced by the public
#	      community string defined in Versanet VN2001/VN2002.
#
	  print LOG "  user at slot $slot port $port: $loginname\n" if ($debug);	  ($loginname eq $ARGV[3]) ? 1 : 0;	
}


# 1999/08/24 Chris Shenton <chris@shenton.org>
# Check Bay8000 NAS (aka: Annex) using finger. 
# Returns from "finger @bay" like:
#   Port  What User         Location         When          Idle  Address
#   asy2  PPP  bill         ---              9:33am         :08  192.168.1.194
#   asy4  PPP  hillary      ---              9:36am         :04  192.168.1.195
#   [...]
# But also returns partial-match users if you say like "finger g@bay":
#   Port  What User         Location         When          Idle  Address
#   asy2  PPP  gore         ---              9:33am         :09  192.168.1.194
#   asy22 PPP  gwbush       ---              Mon  9:19am    :07  192.168.1.80
# So check exact match of username!

sub bay_finger {		# ARGV: 1=nas_ip, 2=nas_port, 3=login, 4=sessid
    open(FINGER, "finger $ARGV[3]\@$ARGV[1]|") || return 2; # error
    while(<FINGER>) {
	my ($Asy, $PPP, $User) = split;
	if( $User =~ /^$ARGV[3]$/ ){
	    close FINGER;
	    print LOG "checkrad:bay_finger: ONLINE $ARGV[3]\@$ARGV[1]"
		if ($debug);
	    return 1; # online
	}
    }
    close FINGER;
    print LOG "checkrad:bay_finger: offline $ARGV[3]\@$ARGV[1]" if ($debug);
    return 0; # offline
}

###############################################################################

# Poor man's getopt (for -d)
if ($ARGV[0] eq '-d') {
	shift @ARGV;
	$debug = "stdout";
}

if ($debug) {
	if ($debug eq 'stdout') {
		open(LOG, ">&STDOUT");
	} elsif ($debug eq 'stderr') {
		open(LOG, ">&STDERR");
	} else {
		open(LOG, ">>$debug");
		$now = localtime;
		print LOG "$now checkrad @ARGV\n";
	}
}

if ($#ARGV != 4) {
	print LOG "Usage: checkrad [-d] nas_type nas_ip " .
			"nas_port login session_id\n" if ($debug);
	print STDERR "Usage: checkrad [-d] nas_type nas_ip " .
			"nas_port login session_id\n"
			unless ($debug =~ m/^(stdout|stderr)$/);
	close LOG if ($debug);
	exit(2);
}

if ($ARGV[0] eq 'livingston') {
	$ret = &livingston_snmp;
} elsif ($ARGV[0] eq 'cisco') {
	$ret = &cisco_snmp;
} elsif ($ARGV[0] eq 'cvx') {
	$ret = &cvx_snmp;
} elsif ($ARGV[0] eq 'multitech') {
        $ret = &multitech_snmp;
} elsif ($ARGV[0] eq 'computone') {
	$ret = &computone_finger;
} elsif ($ARGV[0] eq 'max40xx') {
	$ret = &max40xx_finger;
} elsif ($ARGV[0] eq 'ascend' || $ARGV[0] eq 'max40xx_snmp') {
	$ret = &ascend_snmp;
} elsif ($ARGV[0] eq 'portslave') {
	$ret = &portslave_finger;
} elsif ($ARGV[0] eq 'tc') {
	$ret = &tc_tccheck;
} elsif ($ARGV[0] eq 'pathras') {
	$ret = &cyclades_telnet;
} elsif ($ARGV[0] eq 'pr3000') {
	$ret = &cyclades_snmp;
} elsif ($ARGV[0] eq 'pr4000') {
	$ret = &cyclades_snmp;
} elsif ($ARGV[0] eq 'patton') {
	$ret = &patton_snmp;
} elsif ($ARGV[0] eq 'digitro') {
	$ret = &digitro_rusers;
} elsif ($ARGV[0] eq 'usrhiper') {
	$ret = &usrhiper_snmp;
} elsif ($ARGV[0] eq 'netserver') {
	$ret = &usrnet_telnet;
} elsif ($ARGV[0] eq 'versanet') {
        $ret = &versanet_snmp;
} elsif ($ARGV[0] eq 'bay') {
	$ret = &bay_finger;
} elsif ($ARGV[0] eq 'other') {
	$ret = 1;
} else {
	print LOG "  checkrad: unknown NAS type $ARGV[0]\n" if ($debug);
	print STDERR "checkrad: unknown NAS type $ARGV[0]\n";
	$ret = 2;
}

if ($debug) {
	$mn = "login ok";
	$mn = "double detected" if ($ret == 1);
	$mn = "error detected" if ($ret == 2);
	print LOG "  Returning $ret ($mn)\n";
	close LOG;
}

exit($ret);