#!/usr/bin/perl
use IO;

# mpd.kill $ip-server $nas-port $ip-client
#
if ($ARGV[2] eq ''){ die "Usage: mpd.kill nasip nasport userip"; }

$nasip   = $ARGV[0];
$nasport = $ARGV[1];
$userip  = $ARGV[2];
$nastelnetport = 5005;
 
$sock = IO::Socket::INET->new(
	PeerAddr => $nasip,
	PeerPort => $nastelnetport,
	Proto => 'tcp') or die "Can not connect to mpd!\n$!";

$sock->autoflush(1);

print $sock "link pptp",$nasport,"\n";
print $sock "close\n"; 
print $sock "exit\n"; 
close $sock;
exit 0;
