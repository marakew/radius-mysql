<?

	session_unset();
	session_name("userbilling");
	session_start();

	if ( (isset($_REQUEST['use']) && ($_REQUEST['use'] == "exit")) ||
             (isset($_SESSION['timeout']) && intval((time()-$_SESSION['timeout'])/60) > 1)) {
   		session_unset();
   		session_destroy();
		session_regenerate_id();

   		Header("Location: ./index.php");
#   		exit;
	}

#	$test = intval((time()-$timeout)/60);
#	echo $test;
#	echo "\n";

	$_SESSION['timeout'] = intval(time());
#	echo $timeout;

	require("../inc/config.php");
	echo $admin_header;

	
	if (!isset($user)){

	if (isset($_REQUEST['use']) && $_REQUEST['use'] == "login"){

# $mysql = mysql_connect("$host","$dblogin","$dbpass") or die("Can't connect");
# mysql_select_db("$dbase");
 
 $username = mysql_escape_string($username);
 $query = "SELECT name,passwd FROM userdata WHERE name='$username'";
 $res = mysql_query("$query") or die("Error query $query");
# $row = mysql_fetch_array($query);
 $row = mysql_fetch_row($res);
# if (crypt($password, substr($row[1],0,2)) != $row[1] ){
#	echo "$password $row[1]";
     $password = ntpwdhash($password);
 if (@mysql_num_rows($res) == 1 &&
	$password == $row[1]){

 	$timeout = intval(time());
 	session_register("timeout");

	$user = $row[0];
	session_register("user");

 } else {
	# nothing...
 }
}
	if (!isset($user)){
?>
<body bgcolor="#edede0">
<center><h3>��������� ������������</h3>
<h3><hr size="1" noshade="" align="center" width="50%">
<center>
<form action="<?echo $PHP_SELF?>?use=login" method="post">
<table width="35%" border="0" cellpadding="3" cellspacing="2">
<tbody>
<tr><td width="65%" class="wtxt">������������:</td>
    <td width="45%"><input name="username" class="textbox" size="12" maxlength="15">
</td></tr>
<tr><td width="65%" class="wtxt">������:</td>
    <td width="45%"><input type="password" name="password" class="textbox" size="12" maxlength="15">
</td></tr>
</tbody>
</table>
<hr size="1" noshade="" align="center" width="50%">
<input type="submit" name="report" value="  ����  ">
</form>
<?	exit;
	}

}
?>

<?

 ################## IF NOT LOGED ->and want to hacked! REDIRECT TO 1th PAGE
#if (!session_is_registered('loged') && !isset($use)){
#   session_unset();
#   Header("Location: ./index.php");
#   session_destroy();
#   exit;
#}
?>


<? ################################### MAIN MENU & FRAME ...

    if ( isset($_REQUEST['use'])   &&
        ($_REQUEST['use'] == "login"  ||
         $_REQUEST['use'] == "info"   ||
         $_REQUEST['use'] == "chpass" ||
         $_REQUEST['use'] == "price"  ||
         $_REQUEST['use'] == "stat"   ||
	 $_REQUEST['use'] == "hist"   ||
	 $_REQUEST['use'] == "card" )   ){
  echo $header;
?>
<body bgcolor="#edede0" style="font-family: Helvetica; font-size: 10pt;"><br>
<center><table border="0" cellspacing="0" cellpadding="0" width="85%">
<tbody>
<tr><td valign="top" width="5%"></td></tr>
<!--  main menu  -->
<tr><td valign="top" width="160">
<table border="0" cellpadding="2" cellspacing="3" width="160" class="stxt">
<tbody>
<tr bgcolor="#ffffff"><th>������������</th></tr>
<tr bgcolor="#ffffff"><td align="center"><?echo $user;?></td></tr>
<tr height="25"><td></td></tr>
<tr><td height="2" bgcolor="#ffcc00"></td></tr>
<tr bgcolor="#5050af">
        <th><a href="<?echo $PHP_SELF?>?use=info"><font color="#ffffff">����������</font></a></th></tr>
<tr bgcolor="#5050af">
<!--        <th><a href="<?echo $PHP_SELF?>?use=price"><font color="#ffffff">�����</font></a></th></tr>
-->
<tr bgcolor="#b45252">
        <th><a href="<?echo $PHP_SELF?>?use=card"><font color="#ffffff">���������</font></a></th></tr>
<tr bgcolor="#5050af">
        <th><a href="<?echo $PHP_SELF?>?use=chpass"><font color="#ffffff">������� ������</font></a></th></tr>
<tr bgcolor="#5050af">
        <th><a href="<?echo $PHP_SELF?>?use=stat"><font color="#ffffff">����������</font></a></th></tr>
<tr bgcolor="#5050af">
        <th><a href="<?echo $PHP_SELF?>?use=hist"><font color="#ffffff">�������</font></a></th></tr>
<tr bgcolor="#5050af">
        <th><a href="<?echo $PHP_SELF?>?use=exit"><font color="#ffffff">�����</font></a></th></tr>
<tr><td height="2" bgcolor="#ffcc00"></td></tr>
<tr height="25"><td></td></tr>
<tr bgcolor="#ffffff"><td align="center"><?echo date("d"); echo " ".$monthy[date("m")]." "; echo date("Y: H:i:s");?></td></tr>
</tbody>
</table>
</td>
<!-- end of main menu -->
<?}?>
<!-- example of main frame
<td valign="center" width="95%"><center>
<center><b>EXAMPLE MAIN FRAME</b></center>
</td></center>
-->

<?  ##################################### MAIN FRAME info || login
        if (isset($_REQUEST['use']) &&
           ($_REQUEST['use'] == "info" ||
            $_REQUEST['use'] == "login"   ) ){

# $mysql = mysql_connect("$host","$dblogin","$dbpass") or die("Can't connect");
# mysql_select_db("$dbase");
#				0	1	2   3     4      5     6      7
 $query = mysql_query("SELECT name,round(deposit,2),date_on,gid,mail_cost,mail,block,credit,prefix FROM userdata WHERE name='$user'");
 $row = mysql_fetch_row($query);
 $queryg = mysql_query("SELECT gid,rname,type FROM grp WHERE gid='$row[3]'");
 $rowg = mysql_fetch_row($queryg);
 $uprefix_list = preg_split('//', $row[8], -1, PREG_SPLIT_NO_EMPTY);

# echo "$rowg[0],$rowg[1],$rowg[2]";
?>
<td valign="center" width="95%"><center>
<div class="wtxt"><font color="darkgreen">���������� � ������������</font></div><br>
<table width="80%" border="0" cellpadding="1" cellspacing="2" class="txt">
<tbody>
<tr bgcolor="#e1e1e1">
        <td width="30%" align="right"> ������������:</td>
        <td align="left"><b><?echo $row[0]?></b></td></tr>
<tr>    
	<td align="right">��������:</td>
        <td align="left"><b><font color="#0f4eee"><? foreach ($uprefix_list as $uprefix) echo "<b>$uprefix[0]<b> "; ?></font></b></td></tr>
<tr bgcolor="#e1e1e1">
	<td align="right">�� �����:</td>
<? if ($row[1] <= 0){ ?>
        <td align="left"><b><font color="#ff0000"><?echo $row[1]?></font></b> / <b><?echo $row[6]?></b></td></tr>
<? } else { ?>
        <td align="left"><b><?echo $row[1]?></b> / <b><?echo $row[6]?></b></td></tr>
<? } ?>
<!--
<?
		$query = "SELECT prefix,rname,traf_cost,gid FROM grp WHERE gid = '$row[3]'";
		#echo $query;
		$res = mysql_query("$query") or
				die("Error query <font color=\"RED\">$query</font>");
		$rowgrp = mysql_fetch_array($res);
?>
	<table border="1" cellspacing="0" cellpadding="0" width="80%">
	<tbody>
	<tr>
	<th>������</th>
	<th>�������</th>
	<th>����</th>
	</tr>

	<tr>
	<td><?echo "$rowgrp[1]";?></td>
	<td><?echo "$rowgrp[0]";?></td>
	<td><?echo "$rowgrp[2]";?></td>
	</tr>

	<? foreach ($uprefix_list as $uprefix) { 
		$query = "SELECT prefix,rname,traf_cost,gid FROM grp WHERE prefix = '$uprefix'";
		#echo $query;
		$res = mysql_query("$query") or
				die("Error query <font color=\"RED\">$query</font>");
		$rowgrp = mysql_fetch_array($res);
	?>
	<tr>
	<td><?echo "$rowgrp[1]";?></td>
	<td><?echo "$rowgrp[0]";?></td>
	<td><?echo "$rowgrp[2]";?></td>
	</tr>
	<? } ?>
	</tbody>
	</table>

-->

<!--
<?	if ($rowg[2] != 0 && $rowg[2] != 4){		?>
        <td align="left"><b><a href="<?echo $PHP_SELF?>?use=price&gid=<?echo $row[3];?>"><font color="#ef0fdf">������</font></a></b></td></tr>
<? } else { ?>
        <td align="left"><b>������: <?echo $rowg[0];?></b></td></tr>
<?}		?>
-->

<!--
<? if ($row[4] != 0){ ?>
<tr>    <td align="right">E-Mail:</td>
        <td align="left"><b><?echo $row[5]?></b></td></tr>

<tr bgcolor="#e1e1e1">    
	<td align="right">E-Mail ������:</td>

<? if ($row[4] <= -10){ ?>
        <td align="left"><b><font color="#ff0000"><?echo $row[4]?></font></b></td></tr>
<? } else { ?>
        <td align="left"><b><?echo $row[4]?></b></td></tr>
<? } ?>
-->
<? } ?>

<? if ($row[6] == 1 || $row[1] <= 0 || $row[4] <= -10 ){ ?>
<tr bgcolor="#e1e1e1">
	<td align="right">������:</td>
        <td align="left"><b><font color="#ff0000">������������</font></b></td></tr>
<? } ?>
<!--
        <td align="right">���������:</td>
        <td align="left"><b></b></td></tr>
<tr><td align="right">���:</td>
        <td align="left"><b></b></td></tr>
        <tr bgcolor="#e1e1e1">
        <td align="right">���� �����������:</td>
        <td align="left"><b></b></td></tr>
-->
</tbody></table></center></td>
<? ######################## END of MAIN FRAME info || login
echo $end_frame;
echo $end_head;
exit;
}
?>







<?  ############################## MAIN FRAME chpass & some err...
if (isset($_REQUEST['use']) && $_REQUEST['use'] == "chpass" &&
   (isset($_REQUEST['passwd']) || isset($_REQUEST['passwd2']))){
if ($_REQUEST['passwd'] == $_REQUEST['passwd2']){
# mysql
# $mysql = mysql_connect("$host","$dblogin","$dbpass") or die("Can't connect");
# mysql_select_db("$dbase");
# echo $crypted;
 $passwd = mysql_escape_string($passwd);
# $query = mysql_query("UPDATE userdata SET passwd=encrypt('$passwd') WHERE name='$username'");
 $passwd = ntpwdhash($passwd);
 $query = mysql_query("UPDATE userdata SET passwd='$passwd' WHERE name='$user'");
# or die("error update query");

?>
<td valign="center" width="95%"><center><br><br><center><b>������ ������</b></center></td></center>
<?}else{?>
<td valign="center" width="95%"><center><br><br><center><b>������ �����������</b></center></td></center>
<?}
}

if (isset($_REQUEST['use']) &&
        $_REQUEST['use'] == "chpass" &&
        (!isset($_REQUEST['passwd'])  || !isset($_REQUEST['passwd2'])) ){
?>
<td valign="center" width="95%"><br><br><center>
<form action="<?echo $PHP_SELF?>?use=chpass" method="post" name="main">
<table width="50%" border="0" cellspacing="0" cellpadding="2">
<tbody>
<tr><td colspan="2" align="center" class="wtxt">
        <font color="darkgreen"><b>����� ������ ������������</b></font>
        <br><br></td></tr>
<tr><td class="stxt"><b>����� ������ :</b></td>
    <td><input type="password" name="passwd" class="textbox" size="12"></td></tr>
<tr><td class="stxt"><b>��������� ������ :</b></td>
    <td><input type="password" name="passwd2" class="textbox" size="12"></td></tr>
<tr><td colspan="2" align="center">
    <br><input type="submit" name="submit" value="�������"></td></tr>
</tbody></table></center></td>
<? ###################### END of FRAME chpass
echo $end_frame;
echo $end_head;
exit;
}
?>


<?  ############################## MAIN FRAME chpass & some err...
if (isset($_REQUEST['use']) && $_REQUEST['use'] == "price"){	?>

<td valign="center" width="95%"><center>

<?
	$query = "SELECT name,gid,prefix FROM userdata";
	$query .= " WHERE name = '$user'";
        $resusr = mysql_query("$query") or die("Error query $query");
	$rowusr = mysql_fetch_row($resusr);
	$ugid = $rowusr[1];
	$uprefix_list = preg_split('//', $rowusr[2], -1, PREG_SPLIT_NO_EMPTY);
#	array_push($uprefix_list, "K");


	$query = "SELECT prefix,rname,gid,traf_cost FROM grp";
#	$query .= " WHERE gid = '$gid'";
        $resgrp = mysql_query("$query") or die("Error query $query");

?>
	<table width="90%" border="0" cellpadding="3" cellspacing="2" align="center">
	<tbody>
	<tr class="stxt" bgcolor="#c8c8ff">
	<th>1111111</th>
	<th>2222222</th>
	<th>3333333</th>
	</tr>

<?
		print_r($uprefix_list);

	while ($rowgrp = mysql_fetch_array($resgrp)){
		echo "<tr>";

		foreach ( $uprefix_list as $uprefix ){ 
		echo "<td>$rowgrp[1]</td>";
		echo "<td>$rowgrp[0]</td>";
		echo "<td>$rowgrp[3]</td>";

		}

		echo "</tr>";
?>

<?	# hourgroup price !!!
        $query = "SELECT *  FROM hourgroup WHERE gid='$gid' GROUP BY week";
        $res = mysql_query("$query") or die("Error query $query");
        $rw = mysql_fetch_array($res);
?>
<?	if(mysql_num_rows($res) != 0){	?>
<div class="wtxt"><font color="darkgreen">��������� �����</font></div><br>
<center>
<table width="95%" border="0" cellpadding="1" align="center">
<tbody>
<tr class="stxt" bgcolor="#c8c8ff">
<!-- <th>&nbsp;</th> -->
<th width="15%">\ ��� <font color="#ff0000">�</font></th>
<?for ($i=0; $i<24; $i++){ echo "<th>$i</th>";}?>
</tr>
<tr class="stxt" bgcolor="#c8c8ff">
<!-- <th>&nbsp;</th> -->
<th width="15%">���� \ <font color="#ff0000">��</font></th>
<?for ($i=1; $i<24; $i++){ echo "<th>$i</th>";} echo "<th>0</th>";?>
</tr>
<?      for ($i=0;$i<8;$i++){
                echo "<tr class=\"stxt\" align=\"center\" bgcolor=\"#e1e1e1\">";
		if( $i<7) 
                echo "<td>$wdays[$i]</td>";
		else
                echo "<td><font color=\"#ff0000\"><b>$wdays[$i]</b></font></td>";

        for ($s=0; $s<24; $s++)
                echo "<td width=\"25\" bgcolor=\"\">".$rw[$s]."</td>";
                echo "</tr>";
        $rw = mysql_fetch_array($res);
        }
?>
</tbody>
</table>
<br><center>������ / ���<center>
<? }# else echo "<font color=\"RED\"><b>���</b></font><br><br>"; ?>


<?	# trafgroup price !!!
        $query = "SELECT *  FROM trafgroup WHERE gid='$gid' GROUP BY week";
        $res = mysql_query("$query") or die("Error query $query");
        $rw = mysql_fetch_array($res);
?>
<?	if(mysql_num_rows($res) != 0){	?>
<div class="wtxt"><font color="darkgreen">������������� �����</font></div><br>
<center>
<table width="95%" border="0" cellpadding="1" align="center">
<tbody>
<tr class="stxt" bgcolor="#c8c8ff">
<!-- <th>&nbsp;</th> -->
<th width="15%">\ ��� <font color="#ff0000">�</font></th>
<?for ($i=0; $i<24; $i++){ echo "<th>$i</th>";}?>
</tr>
<tr class="stxt" bgcolor="#c8c8ff">
<!-- <th>&nbsp;</th> -->
<th width="15%">���� \ <font color="#ff0000">��</font></th>
<?for ($i=1; $i<24; $i++){ echo "<th>$i</th>";} echo "<th>0</th>";?>
</tr>
<?      for ($i=0;$i<8;$i++){
                echo "<tr class=\"stxt\" align=\"center\" bgcolor=\"#e1e1e1\">";
		if( $i<7) 
                echo "<td>$wdays[$i]</td>";
		else
                echo "<td><font color=\"#ff0000\"><b>$wdays[$i]</b></font></td>";

        for ($s=0; $s<24; $s++)
                echo "<td width=\"25\" bgcolor=\"\">".$rw[$s]."</td>";
                echo "</tr>";
        $rw = mysql_fetch_array($res);
        }
?>
</tbody>
</table>
<br><center>�������� (������ / ���)<center>
<? } #else echo "<font color=\"RED\"><b>���</b></font><br><br>"; ?>
<? } ?>
<? ###################### END of FRAME price
echo $end_frame;
echo $end_head;
exit;
}
?>

<?
if (isset($_REQUEST['use']) &&
   ($_REQUEST['use'] == "stat" ) ){
?>
<td valign="center" width="95%"><center>
<div class="wtxt"><font color="darkgreen">���������� ����������</font></div><br>

<center>
<table width="95%" border="0" cellpadding="1" cellspacing="1" align="center">
<tbody>
<tr class="stxt" bgcolor="#c8c8ff">
<? if (isset($fmonth)){ ?>
<th width="5%">P</th>
<? } ?>
<th width="25%">����</th>
<? if (isset($fmonth)){ ?>
<th width="35%">����� - ����</th>
<? } else { ?>
<th width="25%">����� - ����</th>
<? } ?>
<th width="25%">�����</th>
<th width="8%">InBytes</th>
<th width="8%">OutBytes</th>
<th width="5%">���������</th>
<? if (!isset($fmonth)){ ?>
<th width="5%">�������</th>
<? } ?>
</tr>

<?
if(isset($fmonth)){
 $query = "SELECT date_format(start_time,'%d %b'),date_format(start_time,'%H:%i:%s'),date_format(stop_time,'%H:%i:%s'),time_on,inbytes,outbytes,round(cost,2),prefix FROM usertime";
# %H:%i:%s'),date_format(stop_time, '%H:%i:%s'),time_on,inbytes,outbytes,cost FROM usertime"; # WHERE name='$username' ORDER BY start_time";
 }else{
  $query = "SELECT date_format(start_time,'%Y %b'),count(time_on),SUM(time_on),SUM(inbytes),SUM(outbytes),round(SUM(round(cost,2)),2),date_format(start_time,'%Y-%m'),date_format(stop_time,'%Y-%m') FROM usertime";
 }
 $query .= " WHERE name='$user'";
 if (isset($fmonth))
 $query .= " AND date_format(start_time,'%Y-%m') = '$fmonth'";
if (!isset($fmonth))
 $query .= " GROUP BY date_format(start_time,'%Y-%m')";
# echo "$query";
 $res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");

  $flagcolor = 0;
while(($row = mysql_fetch_array($res))){
   echo "<tr align=\"center\" class=\"stxt\"";
  if($flagcolor == 1){
   echo " bgcolor=\"#e1e1e1\""; $flagcolor = 0;
   } else {  $flagcolor = 1; }
        echo ">";
if(!isset($fmonth)){
	$query = "SELECT start_time,stop_time,deposit FROM usertime WHERE name = '$user' AND date_format(stop_time,'%Y-%m') = '$row[7]' ORDER BY stop_time DESC LIMIT 1";
	 # echo $query;
	$res_ost = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
	$row_ost = mysql_fetch_array($res_ost);
	$ost = round($row_ost[2], 2);

        $time_on = sprintf("%02d %02d:%02d:%02d",
         floor($row[2]/(3600*24)),floor($row[2]%(3600*24)/3600),floor(($row[2]%3600)/60),floor($row[2]%60));

echo    "<td>$row[0]</td>
         <td><a href=\"$PHP_SELF?action=user&name=$row[0]&use=stat&fmonth=$row[6]\">>> $row[1] <<</td>
         <td>$time_on</td>
         <td>".prts($row[3])."</td>
         <td>".prts($row[4])."</td>
         <td>$row[5]</td>
	 <td>$ost</td>
         </tr>";
}else{

        $time_on = sprintf("%02d %02d:%02d:%02d",
         floor($row[3]/(3600*24)),floor($row[3]%(3600*24)/3600),floor(($row[3]%3600)/60),floor($row[3]%60));

echo	"<td>$row[7]</td>";
echo    "<td>$row[0]</td>
         <td>$row[1] - $row[2]</td>
         <td>$time_on</td>
         <td>".prts($row[4])."</td>
         <td>".prts($row[5])."</td>
         <td>$row[6]</td>
         </tr>";
}

} 

 $query = "SELECT SUM(time_on),SUM(inbytes),SUM(outbytes),round(SUM(round(cost,2)),2),COUNT(time_on) FROM usertime";
 $query .= " WHERE name='$user'";
if (isset($fmonth))
 $query .= " AND date_format(start_time,'%Y-%m') = '$fmonth'";
#if (!isset($fmonth))
# $query .= " GROUP BY date_format(start_time,'%Y-%m')";
 $res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
 $row = mysql_fetch_array($res);

       $time_on = sprintf("%02d %02d:%02d:%02d",
         floor($row[0]/(3600*24)),floor($row[0]%(3600*24)/3600),floor(($row[0]%3600)/60),floor($row[0]%60));
#
#   FIX ME .....  if statistic empty  the message was be corrected
#

   echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#21ae91\">";
	if(isset($fmonth)){
   echo "<td>&nbsp;</td>";
	}

   echo "<td>&nbsp;</td>
         <td>$row[4]</td>
         <td>$time_on</td>
         <td>".prts($row[1])."</td>
         <td>".prts($row[2])."</td>
         <td>$row[3]</td>";
	if (!isset($fmonth)){
   echo "<td>&nbsp;</td>";
	}
   echo "</tr>";

?>
</tbody></table>
<br><br>
</center>


<? 
echo $end_frame; 
echo $end_head;
exit;
}


if (isset($_REQUEST['use']) &&
   ($_REQUEST['use'] == "card" ) ){
?>

<? 
echo $end_frame; 
echo $end_head;
exit;
}

if (isset($_REQUEST['use']) &&
   ($_REQUEST['use'] == "hist" ) ){
?>

<td valign="center" width="95%"><center>
<div class="wtxt"><font color="darkgreen">������� ���������</font></div><br>

<center>
<form method="post" action="<?echo "$PHP_SELF?use=hist";?>">
<table>
<tr><td><select name="fmonth">
<?
 $query = "SELECT date_format(date,'%Y-%m') FROM history GROUP BY date_format(date,'%Y %m')";

        $res = mysql_query("$query") or
                        die("Error query <font color=\"RED\">$query</font>");
 $fselected = 0;
while(($row = mysql_fetch_array($res))){
 if (($fmonth == $row[0] || (date("Y-m") == $row[0] && !isset($fmonth))) && fselected == 0 ){
        echo "<option selected value=\"$row[0]\">$row[0]";
 $fselected = 1;
 }else{
        echo "<option value=\"$row[0]\">$row[0]";
 }
}
?>
</select></td>
<td><input type="submit" value="��������"></td>
</table>
</form>

<?
  if (!isset($fmonth)) $fmonth = date("Y-m");
  $query = "SELECT name,date_format(date,'%d %b %H:%i:%s'),action,operator FROM history";
  $query .= " WHERE date_format(date,'%Y-%m') = '$fmonth'";
  $query .= " AND name = '$user'";
  $query .= " ORDER BY date";

  $res = mysql_query("$query") or
			die("Error query <font color=\"RED\">$query</font>");
?>

<center>
<table width="95%" border="0" cellpadding="1" cellspacing="1" align="center">
<tbody>
<tr class="stxt" bgcolor="#c8c8ff">
	<th width="20%">����</th>
	<th width="55%">��������</th>
</tr>

<?  $flagcolor = 0;
while(($row = mysql_fetch_array($res))){
	echo "<tr align=\"center\" class=\"stxt\"";
	if($flagcolor == 1){
	echo " bgcolor=\"#e1e1e1\">"; $flagcolor = 0;
	} else {
	echo ">";                     $flagcolor = 1;
     }

	echo "<td>$row[1]</td>";
	echo "<td>$row[2]</td>";
?>

</tr>
<? } ?>
</tbody>
</table>
<br><br>
</center>



<? 
echo $end_frame; 
echo $end_head;
exit;
}
?>
