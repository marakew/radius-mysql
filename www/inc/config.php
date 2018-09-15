<?

	$idle_timeout	= 5;

	$host = "localhost";
	$dbase = "data";
	$dblogin = "billing";
	$dbpass = "test123";

 $f = mysql_connect("$host","$dblogin","$dbpass") or 
			die("Error connect to db $dbase as $dblogin@$host");
 $db = mysql_select_db("$dbase") or die("Error use base $dbase");


 $gprefix_list = array();
 $ggid_list = array();

	$query = "SELECT * FROM grp";
	$query .=" ORDER BY gid";
	$res = mysql_query("$query") or 
			die("Error query <font color=\"RED\">$query</font>");
	while ($row = mysql_fetch_array($res)){
		$gprefix_list[$row[prefix]] = $row[rname];
		$ggid_list[$row[gid]] = $row[rname];
	}

#	print_r($gprefix_list);
#	print_r($ggid_list);


function sql_open($query){

 $f = mysql_connect("$host","$dblogin","$dbpass") or die("Error connect");
 $db = mysql_select_db("$dbase") or die("Error select base");
 $result = mysql_query("$query") or die("Error query $query");
 return $result;
}


function ntpwdhash($szPasswd){

$szUnicodePass;
        $nPasswdLen = strlen($szPasswd);
        for( $i = 0; $i < $nPasswdLen; $i++){
                $szUnicodePass .= $szPasswd[$i];
                $szUnicodePass .= "\0";
        }
        $szHash = mhash(MHASH_MD4, $szUnicodePass);
        return strtoupper(bin2hex($szHash));

# E0FBA38268D0EC66EF1CB452D5885E53
#echo "\n".ntpwdhash('abc')."\n";
}
	
function mpdkill($nasport){

$nasip = '127.0.0.1';
$port = '5005';

	if (!($fp = fsockopen("tcp://$nasip", $port))){
	} else {
		fflush($fp);
		fwrite ($fp, "link pptp".$nasport."\n");
		fwrite ($fp, "close\n");
		fwrite ($fp, "exit\n");
		fclose($fp);
	}
}

$blockedname = array('root','admin','bin','daemon',
                   'adm','mail','news','uucp',
                   'operator','ftp','www');

$month = array(
               '01' => 'Январь', '02' => 'Февраль','03' => 'Март',
               '04' => 'Апрель', '05' => 'Май',    '06' => 'Июнь',
               '07' => 'Июль',   '08' => 'Август', '09' => 'Сентябрь',
               '10' => 'Октябрь','11' => 'Ноябрь', '12' => 'Декабрь');


$monthy = array('00' => "00",
                '01' => "Января", '02' => "Февраля",'03' => "Марта",
                '04' => "Апреля", '05' => "Мая",    '06' => "Июня",
                '07' => "Июля",   '08' => "Августа",'09' => "Сентября",
                '10' => "Октября",'11' => "Ноября", '12' => "Декабря");

$wdays = array( 'Воскресенье',
                'Понедельник',
                'Вторник',
                'Среда',
                'Четверг',
                'Пятница',
                'Суббота',
                'Празничный' );

$mdays  = array('1'=>"Пн",
		'2'=>"Вт",
		'3'=>"Ср",
		'4'=>"Чт",
		'5'=>"Пт",
		'6'=>"Сб",
		'7'=>"Вс");


$grptype = array('0' => "Безлимитный",
                 '1' => "Почасовой",
                 '2' => "Почасовой + Траффик",
                 '3' => "Траффик",
                 '4' => "Посуточный");

$grptraf_type = array('0' => "Входящий",
                      '1' => "Суммарный",
                      '2' => "Больший",
                      '3' => "Исходящий",);


$mode_oper = array('0' => "R/O",
                   '1' => "R/W",
                   '2' => "Full Access");


function history_log($uname,$at){
  global $ouser;
  $dtime = date("Y-m-d H:m:i");
  $query = "INSERT INTO history (name,date,action,operator) VALUES ('$uname','$dtime','$at','$ouser')";
  $res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
  return;
}


function print_date($d){

global $monthy;

   if ($d == 0){
#   echo "NA ";
   echo "";
   return;}
#  echo "$d  ";
# list ( $year,$month,$day) = split ("[/.-]", $d);
#   echo "$day $monthy[$month] $year";

if (ereg ("([0-9]{4})[/.-]([0-9]{1,2})[/.-]([0-9]{1,2})",$d,$t)) {
  list ($w,$year,$month,$day) = $t;
#   echo "$day  $month $year $w";
   echo "$day  $monthy[$month] $year г.";
#   echo "$t[3].$t[2].$t[1]";

#   echo "$monthy[$t] ";

}else{
   echo "<font color=\"RED\">$d<font> неверный формат";
}

# 2002-23-12
#   echo "$d ";
#   $t = substr($d,-2); # day
#   echo "$t ";
#   $t = substr($d,5,-3); # month
#   echo "$monthy[$t] ";
#   $t = substr($d,0,4);# year
#   echo "$t";
   return;
}

function print_datetime($d){

#$d ="1994-10-11 13:23:34";

global $monthy;

if (ereg ("([0-9]{4})[/.-]([0-9]{1,2})[/.-]([0-9]{1,2})[ ]([0-9]{2})[:]([0-9]{2})[:]([0-9]{2})",$d,$t)) {
  list ($w,$year,$month,$day,$hour,$min,$sec) = $t;
   echo "$day $monthy[$month] $year г. $hour:$min:$sec";
}else{
   echo "<font color=\"RED\">$d<font> неверный формат";
}
   return;
}


function print_ip($ip){

#$ip = "102.123.102.3";

if(ereg("([0-9]{1,3})[.]([0-9]{1,3})[.]([0-9]{1,3})[.]([0-9]{1,3})",$ip,$t)){
    list ($w,$ip,$ip8,$ip16,$ip32) = $t;
  echo "$ip.$ip8.$ip16.$ip32";
}else
if(!isset($ip) || (isset($ip) && $ip == 0))
  echo "";
 else
  echo "<font color=\"RED\">$ip<font> неверный формат";
return;
}


function radusers(){
	$argrp = array();
	$row = 0;
	$fp = fopen("/usr/local/etc/raddb/users","r");

while (!feof($fp) ) 
while ($data = fgetcsv ($fp, 4000, ",") ) {
   $num = count ($data);
#   print "<p> $num fields in line $row: $data<br>";
   $row++;
   for ($c=0; $c < $num; $c++) {
   $d = $data[$c];
   if( $d !=""){
#       print "($d) ";
#   list ($attr,$value) = split ("[=]",$d);

   ereg("([A-Za-z0-9-]{0,40}) *= *[, \"]([-A-Za-z0-9]{0,40})", $d, $t);

#   list ($attr,$value) = explode ("=,",$d);
#   ereg("([a-zA-z])[=]([a-z0-9A-z])",$d,$t);
#   ereg("([a-z0-9A-z]{1,9})[\ =]([a-z])",$d,$t);
   list ($w,$attr,$value) = $t;
#   print "(w $w a $attr v $value t $t)";
#   print "(a $attr v $value)";

  if (eregi("[Gg][Rr][Oo][Uu][Pp]",$attr)){
#     print "$attr = $value ";
    array_push($argrp,$value);
	}

   }
#       print "($d) ";
   }
#   print "<br>";
}
fclose ($fp);

return $argrp;
}


function prts($st){

for($i=strlen($st),$j=0; $i>=0; $i--,$j++){

	if(($j)%3 || $j == 0 || $j == (strlen($st))){
		$stt = "$st[$i]".$stt;
	} else {
		$stt = "'$st[$i]".$stt;
	}
}
	return (string)$stt;
}

$style ="
<style type=\"text/css\">
.stxt{
        font-family: Helvetica, Helv, Sans-Serif;
        font-size: 10pt ; }
.txt{
        font-family: Arial, Helvetica, Helv, Sans-Serif;
        font-size: 12pt; }
.wtxt{
        font-family: Arial, Helvetica, Helv, Sans-Serif;
        font-size: 12pt; font-weight: bold; }
.bwtxt{
        font-family: Arial, Helvetica, Helv, Sans-Serif;
        font-size: 14pt; font-weight: bold; }
.textbox {
        font-family: Arial,Helvetica, sans-serif;
        font-size: 10pt;
        border-style: solid;
        border-top-width: 1px;
        border-right-width: 1px;
        border-bottom-width: 1px;
        border-left-width: 1px;
        border-color: #6B6B6B }

A:link {
	COLOR: black; FONT-FAMILY: Arial, Helvetica;
	TEXT-DECORATION: none }
A:visited {
	COLOR: black; FONT-FAMILY: Arial, Helvetica;
	TEXT-DECORATION: none }
A:hover {
	COLOR: black;
        FONT-FAMILY: Arial, Helvetica;
	TEXT-DECORATION: none }
</style>";


function print_error($er){
# echo  "<center><br><br>";
 echo  "<table width=60% border=0 cellpaddind=3 cellspacing=2>
        <tr><th align=center><font color=RED>Ошибка !
        <tr><td><hr width=100% size=1 noshade>";
 $er .="<tr><td><hr width=100% size=1 noshade>
        <tr><td align=center>
        <form><input type=button value=\"<< Назад\" OnClick=\"history.back()\"></form><p>";
 echo  "$er";
 echo  "</table>";
 echo  "</center></center></body></html>";
 exit;
}


$header ="
<html><head>
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=koi8-r\">
<meta http-equiv=\"pragma\" content=\"no-cache\">
<meta http-equiv=\"cache-control\" content=\"no-cache\">
<title>Web Interface</title>
$style
</head>";

$admin_header ="
<html><head>
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=koi8-r\">
<meta http-equiv=\"pragma\" content=\"no-cache\">
<meta http-equiv=\"cache-control\" content=\"no-cache\">
<title>Web Interface</title>
$style
</head>
<body bgcolor=\"#edede0\" link=\"#000000\" vlink=\"#000000\" alink=\"#000000\" style=\"font-family: Helvetica; font-size: 12pt;\">";

$end_frame ="</tr></tbody></table>";

$end_menu ="</center>";

$end_head ="</center></body></html>";

?>
