<?
	error_reporting(E_ALL);


	$nasip = '127.0.0.1';
	$nasport ='11';
	$port = '5005';


	if (!($fp = fsockopen("tcp://$nasip", $port))){
	} else {
		fflush($fp);
		fwrite ($fp, "link pptp".$nasport."\n");
		fwrite ($fp, "close\n");
		fwrite ($fp, "exit\n");
		fclose($fp);
	}

	exit(0);


/*
	if (($sock = socket_create(AF_INET, SOCK_STREAM, 0)) < 0){
		echo "socket create failed: reason: ".socket_strerror($sock)."\n";
	}

	if (($ret = socket_connect($sock, $nasip, $port)) < 0){
		echo "socket connect failed: reason: ".socket_strerror($ret)."\n";
	}


	$msg = "link pptp".$nasport."\n";
	socket_write($sock, $msg, strlen($msg));


	$msg = "close\n";
	socket_write($sock, $msg, strlen($msg));

	socket_close($sock);

	exit(0);
*/
?>
