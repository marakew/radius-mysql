<? ##################### Web Interface for billing system ###################
   ###################### last change 16.10.2004 23:14  #####################
   ##########################################################################

	session_unset();
	session_name("adminbilling");
	session_start();

   if((isset($action) && ($action == "exit" )) ||
      (isset($timeout) && intval((time()-$timeout)/60) > 5)){
#	session_start();
	session_unset();
	session_destroy();
	session_regenerate_id();

   } 
	$timeout = intval(time());

	require("../inc/config.php");
	echo $admin_header;

#	echo "$timeout,".intval((time()-$timeout)/60);

#	print_r($_SESSION);
#	print_r($HTTP_SESSION_VARS);
#	print_r($HTTP_COOKIE_VARS);

	if (!isset($ouser)) {

   if (isset($action) && $action == "login"){

	$username = mysql_escape_string($username);
	$query = "SELECT * FROM operators WHERE name='$username'";
	$resouser = mysql_query("$query") or die("Error query $query");
	#echo "$query";
	$puser = mysql_fetch_array($resouser);

	if (@mysql_num_rows($resouser) == 1 && 
		crypt($password, substr($puser[passwd],0,2)) == $puser[passwd]){

	$timeout = intval(time());
	session_register("timeout");

	$ouser = $puser[name];
	session_register("ouser");
	unset($action);
	} else {
		# nothing...
	}
   }
	if (!isset($ouser)){
?>
<body bgcolor="#edede0">
<center><h3>Интерфейс Администратора</h3>
<h3><hr size="1" noshade="" align="center" width="50%">
<center>
<form action="<?echo "$PHP_SELF?action=login";?>" method="post">
<table width="35%" border="0" cellpadding="3" cellspacing="2">
<tbody>
<tr><td width="65%" class="wtxt">Пользователь:</td>
    <td width="45%">
	<input name="username" class="textbox" size="12" maxlength="15">
</td></tr>
<tr><td width="65%" class="wtxt">Пароль:</td>
    <td width="45%">
	<input type="password" name="password" class="textbox" size="12" maxlength="15">
</td></tr>
</tbody>
</table>
<hr size="1" noshade="" align="center" width="50%">
<input type="submit" name="report" value="  Вход  ">
</form>
<?
 echo $end_head; exit;   
	} 

}
?>



<? #################### 1st line: time & userloged ########################## ?>
<center>
<table width="730" align="center" border="0" cellspacing="0" cellpadding="5">
        <tbody>
        <tr class="stxt">
        <td><b><?
	echo date("d"); echo " ".$monthy[date("m")]."  "; echo date("Y H:i:s");
	?></b></td>
	<td align="right"><b>Оператор: <?echo $ouser?> [<a href="<?
		echo "$PHP_SELF?action=exit";	       ?>"> Выход </a>]</b></td>
        </tr>
        </tbody>
</table>
</center>
<? #################### 2nd line: main menu ################################# ?>
<center>
<a href="<?echo "$PHP_SELF";?>">
			<img src="pic/main.gif" border="0"></a>
<a href="<?echo "$PHP_SELF?action=find";?>">
			<img src="pic/find.gif" border="0"></a>
<a href="<?echo "$PHP_SELF?action=newuser";?>">
			<img src="pic/new.gif" border="0"></a>
<a href="<?echo "$PHP_SELF?action=card";?>">
			<img src="pic/card.gif" border="0"></a>
<a href="<?echo "$PHP_SELF?action=test";?>">
			<img src="pic/test.gif" border="0"></a>

<a href="<?echo "$PHP_SELF?action=";
if (isset($action) && $action=="user" && 
    isset($name) && 
    isset($use) && $use == "info" )
     echo "user&name=$name&use=stat\">";
else
     echo "stat\">";
?>
			<img src="pic/stat.gif" border="0"></a>
<a href="<?echo "$PHP_SELF?action=history";?>">
			<img src="pic/history.gif" border="0"></a>
<a href="<?echo "$PHP_SELF?action=online";?>">
			<img src="pic/online.gif" border="0"></a>
<a href="<?echo "$PHP_SELF?action=system";?>">
			<img src="pic/conf.gif" border="0"></a>
</center>
<br>
<br>
<? ##########################  Header of all group ######################
        if (!isset($action) && !isset($stat) ){
?>
<center>
<hr width="55%" size="1" noshade="noshade" align="center">
<table width="55%" border="0" cellpadding="3" cellspacing="2">
<tbody>

<tr bgcolor="#e1e1e1">
	<th align="left" class="wtxt">Категория</th>
	<th class="wtxt">Количество</th>
</tr>
<?
	$query = "SELECT gid,rname FROM grp ORDER BY gid";
	$resgroup = mysql_query("$query") or die("Error query $query");

while(($rowgroup = mysql_fetch_row($resgroup))){
	$gid = $rowgroup[0];

   echo "<tr>";
   echo "<td width=\"45%\" class=\"txt\">";
   echo "<a href=\"$PHP_SELF?action=listing&gid=$gid\">$rowgroup[1]</a>";
   echo "</td>";

	$query = "SELECT COUNT(userdata.gid) FROM userdata,grp WHERE grp.gid='$gid' AND userdata.gid=grp.gid ORDER BY grp.gid";
	$resuser = mysql_query("$query") or die("Error query $query");
	$rowuser = mysql_fetch_row($resuser);
	$usernum = $rowuser[0];

   echo "<td width=\"10%\" align=\"center\" class=\"wtxt\">$usernum</td>";
   echo "</tr>";
}?>
<?
	$query = "SELECT COUNT(userdata.name) FROM userdata WHERE";
	# does't have group's
	#   $query .= " userdata.gid !='$gid' OR";
#	   $query .= " userdata.gid <> grp.gid OR";
	# blocked
	   $query .= " (userdata.block = 1";
	# deposit <= 0.0 or deposit+credit <= 0.0
	   $query .= " OR (userdata.deposit <= 0.01";
	   $query .= " AND (userdata.deposit + userdata.credit) <= 0.01)";
	# date_on >= curent_date()
	   $query .= " OR userdata.date_on > current_date())";
	# order
#	   $query .= " GROUP BY userdata.name";# ORDER BY userdata.name";
	# echo $query;

	   $resuser = mysql_query("$query") or die("Error query $query");
	   $rowuser = mysql_fetch_row($resuser);
	   $userblock = $rowuser[0];
?>
<tr><td width="45%" class="txt">
	<a href="<?echo "$PHP_SELF?action=listing&gid=denied";?>"><font color="DARKRED">Заблокированные</font></a></td>
    <td width="10%" align="center" class="wtxt"><font color="DARKRED"><b><?echo $userblock?><b></b></b></font></td>
</tr>

<?
	$query = "SELECT COUNT(gid) FROM userdata ORDER BY gid";
	$resuser = mysql_query("$query") or die("Error query $query");
	$rowuser = mysql_fetch_row($resuser);
	$usernum = $rowuser[0];
?>
<tr><td width="45%" class="txt">
	<hr size="1" noshade="noshade" align="center"><a href="<?echo $PHP_SELF?>?action=listing&gid=all">All User</a></td>
    <td width="10%" align="center" class="wtxt">
	<hr size="1" noshade="noshade" align="center"><b><?echo $usernum?><b></b></b></td>
</tr>

</tbody>
</table>
</center>
<? echo $end_head; exit; } ### end frame all group	?>


<? ########################## public for Find & Listing #################
   if(isset($action) && ($action == "find" || $action == "listing")){

   if(!isset($order)) $order = "name";

#    $query = "SELECT userdata.name,grp.rname,userdata.deposit,userdata.credit,userdata.date_on,userdata.block FROM userdata,grp";
    $query = "SELECT userdata.name,userdata.gid,round(userdata.deposit,2),userdata.credit,userdata.date_on,userdata.block FROM userdata,grp";

?>

<? ########################## Search User ###############################
        if(isset($action) && $action == "find"){

  if (isset($who)){

  if ($who == "char" && isset($letter)){
    $query .= " WHERE userdata.name LIKE '$letter%'";
 }else
 if ($who == "name" && isset($name)){
    $search  = array("'[*]'e","'[?]'e");
    $replace = array("'%'","'_'");
    $text = preg_replace ($search, $replace, $name);
    $query .= " WHERE userdata.name LIKE '$text'";
 }else{
 }
    $query .= " GROUP BY userdata.name";
    $query .= " ORDER BY userdata.$order";

   if(isset($sort) && $sort =="desc") $query .= " DESC";
                                else  $query .= " ASC";

# echo $query;
	$resuser = mysql_query("$query") or 
			die("Error query <font color=\"RED\">$query</font>");
 $numsearch = mysql_num_rows($resuser);

 }
  if(!isset($numsearch) || (isset($numsearch) && $numsearch == 0)){
  if(isset($numsearch) && $numsearch == 0)
          echo "<center>Совпадений ненайдено</center>";
?>
<center><br><b>Поиск пользователя</b><br><br>
<form action="<?echo "$PHP_SELF?action=find&who=name";?>" method="post">
<input type="text" class="textbox" name="name" size="15" maxlength="20">
<input type="submit" value="Поиск">
</form>
<?for($i='a';$i<'z';$i++) 
  echo "<a href=\"$PHP_SELF?action=find&who=char&letter=$i\"> <b>$i</b></a>";?>
<br><br>
<?for($i=0;$i<10;$i++) 
  echo "<a href=\"$PHP_SELF?action=find&who=char&letter=$i\"> <b>$i</b></a>";?>
</center>
<? echo $end_head; exit;
  } # numsearch
} # who = find
  ######################## end frame Search User ########################
  ############################ if not finded ############################ ?>


<?  ################### Header of user frame ############################
        if(isset($action) && $action == "listing" ){


 if(isset($gid) && $gid != "all" && $gid != "denied")
   $query .= " WHERE userdata.gid = '$gid'";
  else
 if(isset($gid) && $gid == "denied"){
   $query .= " WHERE";
 # does't have his group
#   $query .= " gid !='$rowgroup[0]' AND"; # fuck!!
 # blocked
   $query .= " (userdata.block = 1";
 # deposit <= 0.0 or deposit+credit <= 0.0
   $query .= " OR (userdata.deposit <= 0.01";
   $query .= " AND (userdata.deposit + userdata.credit) <= 0.01)";
 # date_on >= curent_date()
   $query .= " OR userdata.date_on > current_date())";
 }
   $query .= " GROUP BY userdata.name";
   $query .= " ORDER BY userdata.$order";

   if(isset($sort) && $sort =="desc") $query .= " DESC";
                                else  $query .= " ASC";
#  echo $query;
  $resuser = mysql_query("$query") or die("Error query $query");
 } ########################## end build query for listing ##################
?>

<? ############################# view listing or find  #####################
    if(isset($action) && $action == "find"){
      if(isset($who)){ $action .= "&who=$who";
          if($who=="name") $action .= "&name=$name";
         else
           if(isset($letter)) $action .= "&letter=$letter";
      }
    }

    if(isset($sort) && $sort =="desc") $sref ="&sort=asc";
                                  else $sref ="&sort=desc";
    if(isset($gid)) $sref .= "&gid=$gid";
               else $sref .= "";
?>
<br>
<center>
<table width="80%" border="0" cellpadding="3" cellspacing="1">
<tbody>
<tr bgcolor="#c8c8ff" class="stxt">
<th width="20%">
<a href="<?echo "$PHP_SELF?action=$action&order=name$sref";?>">Логин</a></th>
<th width="20%">
<a href="<?echo "$PHP_SELF?action=$action&order=gid$sref";?>">Категория</a></th>
<th colspan="2" width="20%">
<a href="<?echo "$PHP_SELF?action=$action&order=deposit$sref";?>">Счет</a></th>
<th colspan="2" width="20%">
<a href="<?echo "$PHP_SELF?action=$action&order=date_on$sref";?>">Срок работы</a></th>
<th width="20%">
<a href="<?echo "$PHP_SELF?action=$action&order=block$sref";?>">Статус</a></th>
</tr>

<?
  $flagcolor = 0;
while(($rowuser = mysql_fetch_array($resuser))){
   echo "<tr align=\"center\" class=\"stxt\"";
  if($flagcolor == 1){
   echo " bgcolor=\"#e1e1e1\">"; $flagcolor = 0;
   }else{
   echo ">";                     $flagcolor = 1;
 }
   echo "<td><a href=\"$PHP_SELF?action=user&name=$rowuser[0]&use=info\"><b>$rowuser[0]</b></a></td>";

   if ($grname = $ggid_list[$rowuser[1]])
		echo "<td>".$grname."</td>";
	else	echo "<td><font color=RED>Error!! gid ($rowuser[1])</font></td>";

    $deposit = $rowuser[2];
    $credit  = $rowuser[3];
 if($deposit <= 0.01 && ($deposit+$credit) <= 0.01){
   echo "<td><font color=\"DARKRED\"><b>$deposit</b></font></td>";
   echo "<td><font color=\"DARKRED\"><b>$credit</b></font></td>";
 } else
   if($deposit <= 0.01){
   echo "<td><font color=\"DARKRED\"><b>$deposit</b></font></td>";
   echo "<td>$credit</td>";
 } else
   echo "<td>$deposit</td><td>$credit</td>";

  if($rowuser[4] == 0)  echo "<td>NA</td><td>NA></td>";
                 else   echo "<td>$rowuser[4]</td><td>&nbsp;</td>";
  if($rowuser[5] == 1)  echo "<td><font color=\"DARKRED\"><b>Блок</b></font></td>";
                 else   echo "<td></td>";
   echo "</tr>";
}?>

</tbody>
</table>
</center>
<? echo $end_head; exit; }
 ########################## end frame listing ?? or find ##################
 ########################## end public for Find & Listing ################# ?>


<? ########################## User ###############################
        if(isset($action) && $action == "user" && isset($name)){

# start_user_block
   if(isset($use) && $use == "block")
   if (isset($block)){
#    if (!empty($block)){
	$query = "UPDATE userdata set block = '$block' WHERE name='$name'";
	$res = mysql_query("$query") or die("Error query $query");
    $msg = "Пользователь: <b>$name</b> - ";
    $msg .= ($block == 0) ? "<b>Разблокирован</b>":"<b>Заблокирован</b>";
    history_log($name,$msg);
   $use = "info";
   } else {
	echo "Error block";
#   $use = "info";
   } 
# end_user_block

# start_user_delete
   if(isset($use) && $use == "delete")
   if(isset($delete) && $delete == "1"){
	$query = "DELETE FROM userdata WHERE name='$name'";
	$res = mysql_query("$query") or die("Error query $query");

    $msg = "Пользователь: <b>$name</b> - <b>Удален</b>";
    history_log("- -",$msg);
    } else {
     $use = "info";
    } 
# end_user_delete

# start_user_passwd
   if(isset($use) && $use == "chpass" && (isset($passwd) || isset($passwd2))){
	if ($passwd == $passwd2 && strlen($passwd) >= 6){
		$passwd = mysql_escape_string($passwd);
		$passwd = ntpwdhash($passwd);
	$query = "UPDATE userdata SET passwd='$passwd' WHERE name='$name'";
  	$res = mysql_query("$query") or die("Error query $query");
     $use = "info";
	   } else {
     $use = "fpasswd";
	if ($passwd != $passwd2)
		$msg = "несовпадают";
	if (strlen($passwd) < 6)
		$msg .= " должен быть неменее 6 символов";
?>
<td valign="center" width="95%"><center><br><br><center><b><?echo $msg?></b></center></td></center>
<?
  	  }
	}
# end_user_passwd

# start_user_ip
   if(isset($use) && $use == "ip"){
	$use = "info";
   }
# stop_user_ip

# start_user_dateon
   if(isset($use) && $use == "dateon"){
	$use = "info";
   }
# stop_user_dateon

# start_user_maildelete
   if(isset($use) && $use == "maildelete"){

	$query = "UPDATE userdata SET mail='',mail_cost=0 WHERE name='$name'";
  	$res = mysql_query("$query") or die("Error query $query");
	$use = "info";
   }
# stop_user_maildelete

# start_user_expired
   if(isset($use) && $use == "expired"){
	$use = "info";
   }
# stop_user_expired

# start_user_expcredit
   if(isset($use) && $use == "expcredit"){
	$use = "info";
   }
# stop_user_expcredit

# start_user_credit

   if(isset($use) && $use == "credit"){
   	if(isset($money) && is_numeric($money)){
	 $query = "SELECT credit FROM userdata WHERE name='$name'";
	 $res = mysql_query("$query") or die("Error query $query");
	 $row = mysql_fetch_array($res);
	 $num = mysql_num_rows($res);

	 $query = "UPDATE userdata set credit ";
	if($money == "0"){
	 	$query .= "= '0' WHERE name='$name'";
	}else{
	 	$query .= "= credit + '$money' WHERE name='$name'";
	}
	$res = mysql_query("$query") or die("Error query $query");
	$msg = "Кредит: <b>$money</b> "; 
	history_log($name,$msg);

	 $use = "info";
	}
   }
# end_user_credit

# start_user_deposit

   if(isset($use) && $use == "deposit")
   if(isset($money) && is_numeric($money)){
    $query_m = "SELECT deposit,mail_cost FROM userdata WHERE name='$name'";
    $res_m = mysql_query("$query_m") or die("Error query $query");
    $row_m = mysql_fetch_array($res_m);
    $num_m = mysql_num_rows($res_m);

    if($num_m == 1){

   if($row_m[1] >= 0){
   if($money == "0"){
  $query = "UPDATE userdata set deposit = '0' WHERE name='$name'";
     $msg = "Оплата: <b>Депозит обнулен</b> "; 
    }else{
  $query = "UPDATE userdata set deposit = deposit + '$money' WHERE name='$name'";
     $msg = "Оплата: <b>$money</b> "; 
	}
   }

   if($row_m[1] < 0){
       $money_m = $money + $row_m[0] + $row_m[1];
       if ($money_m <0 ){
     $query = "UPDATE userdata set deposit = '0',mail_cost = '$money_m'  WHERE name='$name'";
     $msg = "Оплата: <b>$money</b>. Депозит <b>0</b> за E-Mail <b>$money_m</b>"; 
       }else{
     $query = "UPDATE userdata set deposit = '$money_m',mail_cost = '10',block = 0 WHERE name='$name'";
     $msg = "Оплата: <b>$money</b>. Депозит <b>$money_m</b> за E-Mail <b>10</b>"; 
       }
   }
	$res = mysql_query("$query") or die("Error query $query");
    history_log($name,$msg);
    }
    $use = "info";
  } else {
    $use = "info";
  } 
# end_user_deposit

# start_user_prefix
   if(isset($use) && 
	(($use == "prefix" || $use == "addprefix" || $use == "delprefix") && isset($prefix) )){

	$uprefix_str = "";

	$query = "SELECT * from userdata WHERE name='$name'";
	$res = mysql_query("$query") or die("Error query $query");
	$row = mysql_fetch_array($res);

	$uprefix_list = preg_split('//', $row['prefix'], -1, PREG_SPLIT_NO_EMPTY);

	if ($use == "addprefix"){
		array_push($uprefix_list, $prefix);
		$uprefix_list = array_unique($uprefix_list);
		$msg_prefix = "Добавлен";
	}

	if ($use == "delprefix"){
		$uprefix_list_new = array();
		foreach ($uprefix_list as $uprefix){
			if ($uprefix != $prefix)
			array_push($uprefix_list_new, $uprefix);
		}
		$uprefix_list = array_unique($uprefix_list_new);
		$msg_prefix = "Удален";
	}

	$uprefix_str = join($uprefix_str, $uprefix_list);

	$query = "UPDATE userdata SET prefix='$uprefix_str' WHERE name='$name'";
	$res = mysql_query("$query") or die("Error query $query");

	$msg = $msg_prefix." - Новый Префикс <b>".$uprefix_str."</b>";
	history_log("$name",$msg);

     $use = "info";
   }
# end_user_prefix

# start_user_group
   if(isset($use) && $use == "group" && isset($gid)){

	$query = "UPDATE userdata SET gid='$gid' WHERE name='$name'";
	$res = mysql_query("$query") or die("Error query $query");

	$query = "SELECT * FROM grp WHERE gid='$gid'";
	$res = mysql_query("$query") or die("Error query $query");
	$row = mysql_fetch_array($res);

	$msg = "Группа <b>".$row['rname']."</b>";
	history_log("$name",$msg);
     $use = "info";
   }
# end_user_group

# start_user_add
   if(isset($use) && $use == "add"){
  $error = "";

        for ($i=0;$i<count($blockedname);$i++)
         if ($blockedname[$i] == $name){
          $error .= "<tr><td>Вы не можете использовать этот Логин.";
          $result = 1;
          }

  $query = "SELECT name FROM userdata WHERE name='$name'";
  $res = mysql_query("$query") or die("Error query $query");
  $num = mysql_num_rows($res);

        if ($num != 0){
         $error .= "<tr><td>Такой Логин уже существует.";
         $result = 1;
         }

        if (empty($name) || strlen($name)<2 || strlen($name)>12){
         $error .= "<tr><td>Неправильно выбран Логин.";
         $result = 1;
        }

        if (empty($passwd) || strlen($passwd)<6){
         $error .= "<tr><td>Пароль должен быть не менеее 6 символов.";
         $result = 1;
        }

 #       if (ereg("[A-Z]",$name) ) {
        if (ereg("[^a-z0-9\._\-]",$name) ) {
         $error .= "<tr><td>Логин может содержать только a-z0-9._-";
         $result = 1;
        }

#       $firstchar = substr($name,0,1);
#       if (ereg("[0-9]",$firstchar)) {
#        $error .= "<tr><td>Первый символ Логина не должен начинаться с цифры.";
#        $result = 1;
#        }

        if (isset($result) && $result == 1) print_error($error);
         else{

       $msg = "Новый Пользователь: <b>$name</b>";
  if (isset($use_exp))
          $expired = "$year_exp-$month_exp-$day_exp $hour_exp:$min_exp:00";
     else{ $expired = "0000-00-00 00:00:00"; $exp_credit = 0;}

  if (!isset($use_deposit) ){ 
		$deposit = 0; $credit= 0;
	}else{
   }

  if (isset($use_activate)){
		$activate = "$year_actv-$month_actv-$day_actv";
         }else{ 
		$activate = date("Y-m-d");  
	 }

  if(empty($mail)){
   $mail = "";
   $mail_cost = 0 ; 
		$msg .= " Депозит <b>$deposit</b> Кредит <b>$credit</b>";
  }else{
   $deposit -= 10;
   $mail_cost = 10;
		$msg .= " Депозит <b>$deposit</b> Кредит <b>$credit</b>";
   		$msg .= " За E-Mail 10";
  }
 $passwd = ntpwdhash($passwd); # ,encrypt('$passwd'),
$query = "INSERT INTO userdata (name,passwd,gid,deposit,credit,rname,expired,exp_credit,block,callback,prefix,framed_ip,allow_phone,address,phone,date_on,prim,mail_cost,mail) values('$name','$passwd','$gid','$deposit','$credit','$rname','$expired','$exp_credit','0','$callback','','$framed_ip','','$address','$phone','$activate','$prim','$mail_cost','$mail')";
# echo $query;
  $res = mysql_query("$query") or die("Error query $query");

  history_log($name,$msg);
#  echo "<center><br><br><b>Пользователь <b>$name<b> создан.<br><br>";
  }

 } 

# end_user_add
?>

<?
	if(isset($use) && $use == "fblock"){
?>
<center><br><b><font color=DARKGREEN>Изменение Статуса Пользователя</font><br><br>
Пользователь: <font color=DARKRED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=block";?>" method=post>
<table border=0 class=txt cellpadding=3 cellspacing=2>
<tr><td><input type=radio name=block value=1 CHECKED><td> Блокировать</td>
<tr><td><input type=radio name=block value=0><td> Разблокировать</td>
</table>
<br><br><input type=submit name=report value=Изменить><br></form>
<?
   }
	if(isset($use) && $use == "fdelete"){
?>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=delete";?>" method=post>
<input type=checkbox name=delete value=1> &nbsp;
<b><font color=DARKGREEN>Удалить</font></b>
<br><br><input type=submit name=report value=Изменить><br></form>
<?
   }
	if(isset($use) && $use == "fpasswd"){
?>
<td valign="center" width="95%"><br><br><center>
<form action="<?echo "$PHP_SELF?action=user&use=chpass&name=$name";?>" method="post" name="main">
<table width="50%" border="0" cellspacing="0" cellpadding="2">
<tbody>
<tr><td colspan="2" align="center" class="wtxt">
<font color="darkgreen"><b>Смена Пароля пользователя <font color=DARKED><?echo $name;?></font></b></font>
<br><br></td></tr>
<tr><td class="stxt"><b>Новый пароль :</b></td>
	<td><input type="password" name="passwd" class="textbox" size="12"></td></tr>
<tr><td class="stxt"><b>Повторите пароль :</b></td>
	<td><input type="password" name="passwd2" class="textbox" size="12"></td></tr>
<tr><td colspan="2" align="center">
	<br><input type="submit" name="submit" value="Сменить"></td></tr>
</tbody></table></center></td>
<?
   }


# start_user_prefix

	if(isset($use) && $use == "fprefix"){
		$grplist = array();
		$grp_prefix = array();
		$grp_rname = array();

		$uprefix_list = array();

	 $query = "SELECT prefix FROM userdata WHERE name='$name'";
	 $res = mysql_query("$query") or die("Error query $query");
	 $row = mysql_fetch_array($res);
#	 while ($row = mysql_fetch_array($res)){
#		array_push($uprefix_list, $row['prefix']);
	$uprefix_list = preg_split('//', $row['prefix'], -1, PREG_SPLIT_NO_EMPTY);
#	 }

	 $query = "SELECT rname,prefix FROM grp ORDER BY gid";
	 $res = mysql_query("$query") or die("Error query $query");

	while ($rowgrp = mysql_fetch_array($res)){
		array_push($grp_prefix, $rowgrp['prefix']);
		array_push($grp_rname, $rowgrp['rname']);
#		$grplist["$grp_
	}
	$grplist = array_map(null, $grp_prefix, $grp_rname);
#	print_r($grplist);
?>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<table border=0 class=txt cellpadding=3 cellspacing=2>
<tr>
<td align="center" class="wtxt" width="5%">
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=delprefix";?>" method=post>
<select name="prefix" class="textbox" size="10">
        <?foreach ($uprefix_list as $uprefix)
              echo "<option value=\"$uprefix[0]\">$uprefix[0]</option>";?>
        </select>
<br><br><input type=submit name=report value=Удалить><br></form>
</td>
<!-- <td nowrap="0" colspan="0" align="center" width="5%">&nbsp;</td> -->
<td align="center" class="wtxt" width="5%">
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=addprefix";?>" method=post>
<select name="prefix" class="textbox" size="10">
        <?foreach ($gprefix_list as $gprefix => $gname)
	   echo "<option value=\"$gprefix\">$gprefix - $gname</option>";?>
        </select>
<br><br><input type=submit name=report value=Добавить><br></form>
</td>
</table>
<?
   }

# end_user_prefix

	if(isset($use) && $use == "fgroup"){

	$query = "SELECT * from userdata WHERE name='$name'";
	$res = mysql_query("$query") or die("Error query $query");
	$numrow = mysql_num_rows($res);
	$row = mysql_fetch_array($res);

?>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=group";?>" method=post>
<select name="gid" class="textbox">
        <?foreach ($ggid_list as $ggid => $gname)
              if ($ggid == $row['gid']) 
		   echo "<option selected value=\"$ggid\">$gname</option>";
              else echo "<option value=\"$ggid\">$gname</option>";?>
        </select>
<br><br><input type=submit name=report value=Изменить><br>
</form>
<?
   }
	if(isset($use) && $use == "fip"){
?>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=ip";?>" method=post>

<br><br><input type=submit name=report value=Изменить><br></form>
<?
   }
	if(isset($use) && $use == "fdateon"){
?>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=dateon";?>" method=post>

<br><br><input type=submit name=report value=Изменить><br></form>
<?
   }
	if(isset($use) && $use == "fmaildelete"){
?>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=maildelete";?>" method=post>

<br><br><input type=submit name=report value=Удалить><br></form>
<?
   }
	if(isset($use) && $use == "fexpired"){
?>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=expired";?>" method=post>

<br><br><input type=submit name=report value=Изменить><br></form>
<?
   }
	if(isset($use) && $use == "fexpcredit"){
?>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=expcredit"?>" method=post>

<br><br><input type=submit name=report value=Изменить><br></form>
<?
   }
	if(isset($use) && $use == "fdeposit"){
?>
<center><br><b><font color=DARKGREEN>Счет (Депозит)</font><br>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=deposit";?>" method=post>
Добавить / Снять:
<input name=money class=textbox size=5 maxlenght=10>
<input type=hidden name=mmoney value=0>
<br><br><font class=stxt>При вводе <b>0</b> депозит обнулится</font>
<br><br><input type=submit name=report value=Изменить><br></form>
<?
   }
	if(isset($use) && $use == "fcredit"){
?>
<center><br><b><font color=DARKGREEN>Счет (Кредит)</font><br>
<center><br><b>Пользователь: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=user&name=$name&use=credit";?>" method=post>
Добавить / Снять:
<input name=money class=textbox size=5 maxlenght=10>
<input type=hidden name=mmoney value=0>
<br><br><font class=stxt>При вводе <b>0</b> кредит обнулится</font>
<br><br><input type=submit name=report value=Изменить><br></form>
<?
   }
	if (isset($use) && ($use == "add" || $use == "info") ){

	$query = "SELECT * from userdata WHERE name='$name'";
	$res = mysql_query("$query") or die("Error query $query");
	$numuser = mysql_num_rows($res);
	$row = mysql_fetch_array($res);

#	$uprefix_list  = array();
#	 while ($row = mysql_fetch_array($res)){
#		array_push($uprefix_list, $row['prefix']);
#	 }
	$uprefix_list = preg_split('//', $row['prefix'], -1, PREG_SPLIT_NO_EMPTY);

	$query = "SELECT rname,gid FROM grp WHERE gid='".$row['gid']."' ORDER BY gid";
	$res = mysql_query("$query") or die("Error query $query");
	$numgrp = mysql_num_rows($res);
	if ($numgrp == 1)
		$rowgrp = mysql_fetch_array($res);
	else	
		$rowgrp['rname'] = "<font color=\"RED\">Error!! gid (".$row['gid']." )</font>";



       if ($numuser == 1){
?>
<hr width="70%" size="1" noshade="" align="center">
<center>
<table width="70%" border="0" cellpadding="3" cellspacing="2" class="stxt">
<tbody>
<tr><td width="25%">Пользователь:</td>
    <td width="25%"><b><?echo $name?></b></td>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fdelete";?>">
			<img src="pic/delete.gif" border=0 alt=""></a></td>
</tr>
<tr><td width="25%">Префикс:</td>
    <td width="25%"><? foreach ($uprefix_list as $uprefix) echo "<b>$uprefix[0]<b> "; ?></td>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fprefix";?>">
			<img src="pic/prefix.gif" border=0 alt=""></a></td>
</tr>
<tr><td width="25%">Пароль:</td>
    <td width="25%"><b>******</b></td>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fpasswd";?>">
			<img src="pic/chpasswd.gif" border=0 alt=""></a></td>
</tr>
<tr><td width="25%">Категория:</td>
    <td width="25%"><b><?echo $rowgrp['rname']?></b></td>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fgroup";?>">
			<img src="pic/chgroup.gif" border=0 alt=""></a></td>
</tr>
<tr><td width="25%">Счет:</td>
    <td width="25%"><b><?echo round($row['deposit'],2)?> / <?echo $row['credit']?></b></td>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fcredit";?>">
			<img src="pic/credit.gif" border=0 alt=""></a>
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fdeposit";?>">
			<img src="pic/chdeposit.gif" border=0 alt=""></a></td>
</tr>
<tr><td width="25%">Срок Действия:</td>
    <td width="25%"><b>
<? if($row['expired'] == 0) echo "NA";
 else {
 print_datetime($row['expired']);
 echo " / ".$row['exp_credit'];
 }?>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fexpcredit";?>">
			<img src="pic/credit.gif" border=0 alt=""></a>
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fexpired";?>">
			<img src="pic/chexpired.gif" border=0 alt=""></a></td>
</b></td>
</tr>
<tr><td width="25%">Статус:</td>
    <td width="25%"><b><?if($row['block'] == 1) echo "<font color=\"RED\">Заблокирован</font>";else echo "Рабочий";?></b></td>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fblock";?>">
			<img src="pic/block.gif" border=0 alt=""></a></td>
</tr>
<tr><td width="25%">IP:</td>
    <td width="25%"><b><?print_ip($row['framed_ip']);?></b></td>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fip";?>">
			<img src="pic/ip.gif" border=0 alt=""></a></td>
</tr>
<!-- <tr>
<td width="40%">Последнее соединение:</td>
<td width="60%"><b>30 Декабря 2002 г. 17:59 </b></td>
</tr> -->
<tr><td width="25%">Дата подключения:</td>
    <td width="25%"><b><?print_date($row['date_on']);?></b></td>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fdateon";?>">
			<img src="pic/podkl.gif" border=0 alt=""></a></td>
</tr>
<? if (!empty($row['mail']) || $row['mail_cost'] != 0){ ?>
<tr>
<td width="25%">e-mail</td>
<td width="25%"><b><?echo $row['mail']?></b></td>
</tr>
<tr><td width="25%">Стоимость:</td>
    <td width="25%"><b><?echo $row['mail_cost']?> грн./мес</b></td>
    <td align="right">
    <a href="<?echo "$PHP_SELF?action=user&name=$name&use=fmaildelete";?>">
			<img src="pic/delete.gif" border=0 alt=""></a></td>
</tr>
<? } ?>
</tbody>
</table>

<hr width="60%" size="1" noshade="" align="center">
<table width="50%" border="0" cellpadding="3" cellspacing="2" class="stxt">
<tbody>
<tr><td width="40%">Имя:</td>
    <td width="60%"><b><?echo $row['rname']?></b></td>
</tr>
<tr><td width="40%">Телефон:</td>
    <td width="60%"><b><?echo $row['phone']?></b></td>
</tr>
<tr><td width="40%">Адрес:</td>
    <td width="60%"><b><?echo $row['address']?></b></td>
</tr>
<tr><td width="40%">Примечания:</td>
    <td width="60%"><b><pre><?if(isset($row['prim'])) echo $row['prim'];?></pre></b></td>
</tr>
</tbody>
</table>
<hr width="60%" size="1" noshade="" align="center">
<?} else {
	echo "<center>Такого бользователя нет в БД</center>";
}

} ?> 

      <?  if (isset($use) && $use == "stat" ){ ?>
<center>
<table width="95%" border="0" cellpadding="3" cellspacing="2" align="center">
<tbody>

<tr class="stxt" bgcolor="#c8c8ff">
<th width="1%">P</th>
<th width="10%">Имя</th>
<th width="25%">Дата</th>
<th width="25%">Старт - Стоп</th>
<th width="18%">Время</th>
<th width="8%">InBytes</th>
<th width="8%">OutBytes</th>
<th width="5%">Стоимость</th>
<? if (!isset($fmonth)){ ?>
<th width="5%">Остаток</th>
<? } ?>
<th width="10%">ip</th>
<th width="4%">port</th>
</tr>
<?
if (isset($fmonth)){
 $query = "SELECT name,date_format(start_time, '%d %b'),date_format(start_time,'%H:%i:%s'),date_format(stop_time, '%H:%i:%s'),time_on,inbytes,outbytes,round(cost,2),ip,port,prefix FROM usertime";
} else {
 $query = "SELECT name,date_format(start_time,'%b'),count(time_on),SUM(time_on),SUM(inbytes),SUM(outbytes),round(SUM(round(cost,2)),2),date_format(start_time,'%Y-%m'),date_format(stop_time,'%Y-%m') FROM usertime";
}
 $query .= " WHERE name='$name'";
if (isset($fmonth))
 $query .= " AND date_format(start_time,'%Y-%m') = '$fmonth'";
if (!isset($fmonth))
 $query .= " GROUP BY date_format(start_time,'%Y-%m')";
# $query .= " ORDER BY start_time";
	$res = mysql_query("$query") or 
			die("Error query <font color=\"RED\">$query</font>");

  $flagcolor = 0;
while(($row = mysql_fetch_array($res))){
   echo "<tr align=\"center\" class=\"stxt\"";
  if($flagcolor == 1){
   echo " bgcolor=\"#e1e1e1\""; $flagcolor = 0;
   } else {  $flagcolor = 1; }
	echo ">";
	$time_on = sprintf("%02d %02d:%02d:%02d",
	 floor($row[4]/(3600*24)),floor($row[4]%(3600*24)/3600),floor(($row[4]%3600)/60),floor($row[4]%60));
if(!isset($fmonth)){

	$query = "SELECT start_time,stop_time,deposit FROM usertime WHERE name = '$row[0]' AND date_format(stop_time,'%Y-%m') = '$row[8]' ORDER BY stop_time DESC LIMIT 1";
	# echo $query;
 	$res_ost = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
	$row_ost = mysql_fetch_array($res_ost);
	$ost = round($row_ost[2], 2);

	$time_on = sprintf("%02d %02d:%02d:%02d",
	 floor($row[3]/(3600*24)),floor($row[3]%(3600*24)/3600),floor(($row[3]%3600)/60),floor($row[3]%60));
echo	"<td>&nbsp;</td>
	 <td><a href=\"$PHP_SELF?action=user&name=$row[0]&use=info\"><b>$row[0]</b></a></td>
	 <td>$row[1]</td>
	 <td><a href=\"$PHP_SELF?action=user&name=$row[0]&use=stat&fmonth=$row[7]\">>> $row[2] <<</td>
	 <td>$time_on</td>
	 <td>".prts($row[4])."</td>
	 <td>".prts($row[5])."</td>
	 <td>$row[6]</td>
	 <td>$ost</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 </tr>";
} else {
echo 	"<td>$row[10]</td>
	 <td>$row[0]</td>
	 <td>$row[1]</td>
	 <td>$row[2] -- $row[3]</td>
	 <td>$time_on</td>
	 <td>".prts($row[5])."</td>
	 <td>".prts($row[6])."</td>
	 <td>$row[7]</td>
	 <td>$row[8]</td>
	 <td>$row[9]</td>
	 </tr>";
}
} 

 $query = "SELECT SUM(time_on),SUM(inbytes),SUM(outbytes),round(SUM(round(cost,2)),2),COUNT(time_on) FROM usertime";
 $query .= " WHERE name='$name'";
if (isset($fmonth))
 $query .= " AND date_format(start_time,'%Y-%m') = '$fmonth'";
#if (!isset($fmonth))
# $query .= " GROUP BY date_format(start_time,'%Y-%m')";
 $res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
 $row = mysql_fetch_array($res);

	$time_on = sprintf("%02d %02d:%02d:%02d",
	 floor($row[0]/(3600*24)),floor($row[0]%(3600*24)/3600),floor(($row[0]%3600)/60),floor($row[0]%60));

   echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#21ae91\">
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>$row[4]</td>
	 <td>$time_on</td>
	 <td>".prts($row[1])."</td>
	 <td>".prts($row[2])."</td>
	 <td>$row[3]</td>";
if (!isset($fmonth)){ 
   echo "<td>&nbsp;</td>";
	}
   echo "<td>&nbsp;</td>
	 <td>&nbsp;</td>
	 </tr>";
?>
</tbody></table>
<br><br>
</center>
<? } ?>

<?  echo $end_head; exit; }
  ######################## end frame User #####################
?>




<?  ######################## Form New User #################################
        if(isset($action) && ($action == "newuser")){
?>
<center>
<form action="<?echo "$PHP_SELF?action=user&use=add";?>" method="post">
<hr width="75%" size="1" noshade="" align="center">
<table width="75%" border="0" cellpadding="3" cellspacing="2" class="stxt">
<tbody>
<tr><td width="25%">Пользователь</td>
    <td width="75%"><input class="textbox" name="name" value="" size="12" maxlength="32"></td>
</tr>
<tr><td width="25%">Пароль:</td>
    <td width="75%"><input class="textbox" type="password" name="passwd" size="12" maxlength="32"></td>
</tr>
<tr><td width="25%">Категория</td>
    <td width="75%">
<select name="gid" class="textbox">
<?foreach ($ggid_list as $gid => $grname)
     echo "<option value=\"$gid\">$grname</option>";
?>
</select></td>
</tr>
<tr><td width="25%">Счет <b> /</b> Кредит</td>
    <td width="75%">
      <input type="checkbox" name="use_deposit" value="1" onclick="
      if (use_deposit.checked != true ){
          deposit.disabled=true; credit.disabled=true;
          deposit.value=0;       credit.value=0;
      }else{
          deposit.disabled=false;credit.disabled=false;
      }">
      <input disabled name="deposit" class="textbox" value="0" size="5"> /
      <input disabled name="credit"  class="textbox" value="0" size="5">
     </td>
</tr>
<tr><td width="25%">Срок Действия</td>
    <td width="75%">
    <input type="checkbox" name="use_exp" value="1"
    onclick="
     if (use_exp.checked != true ){
            year_exp.disabled=true;
            month_exp.disabled=true;
            day_exp.disabled=true;
            hour_exp.disabled=true;
            min_exp.disabled=true;
            exp_credit.disabled=true;exp_credit.value=0;
    }else{
            year_exp.disabled=false;
            month_exp.disabled=false;
            day_exp.disabled=false;
            hour_exp.disabled=false;
            min_exp.disabled=false;
            exp_credit.disabled=false;
    }">
<select disabled name="year_exp" class="textbox">
    <?for($i=2001;$i<2010;$i++)
      if ($i == date("Y")) echo "<option selected value=\"$i\">$i</option>";
                      else echo "<option value=\"$i\">$i</option>";?>
</select>
<select disabled name="month_exp" class="textbox">
    <?foreach ($month as $i => $m)
      if ($i == date("m")) echo "<option selected value=\"$i\">$m</option>";
                      else echo "<option value=\"$i\">$m</option>";?>
</select>
<select disabled name="day_exp" class="textbox">
    <?for($i=1;$i<32;$i++)
      if ($i == date("d")) echo "<option selected value=\"$i\">$i</option>";
                      else echo "<option value=\"$i\">$i</option>";?>
</select>
<select disabled name="hour_exp" class="textbox">
    <?for($i=0;$i<24;$i++)
      if ($i == date("H")) echo "<option selected value=\"$i\">$i</option>";
                      else echo "<option value=\"$i\">$i</option>";?>
</select>:<select disabled name="min_exp" class="textbox">
    <?for($i=0;$i<60;$i++)
      if ($i == date("i")) echo "<option selected value=\"$i\">$i</option>";
                      else echo "<option value=\"$i\">$i</option>";?>
</select>
<b>/</b><input disabled name="exp_credit" class="textbox" value="0" size="2">
<!-- </td>
</tr>
<tr><td width="25%">Кредит</td>
    <td width="75%">
    <input name="exp_credit" class="textbox" value="0" size="1">
-->
</td>
</tr>
<tr><td width="25%">Дата Активации :</td>
    <td width="75%"><input type="checkbox" name="use_activate" value="1"
  onclick="
     if (use_activate.checked != true){
            year_actv.disabled=true;
            month_actv.disabled=true;
            day_actv.disabled=true;
      }else{
            year_actv.disabled=false;
            month_actv.disabled=false;
            day_actv.disabled=false;
    }">
    <select disabled name="year_actv" class="textbox">
    <?for($i=2001;$i<2010;$i++)
      if ($i == date("Y")) echo "<option selected value=\"$i\">$i</option>";
                      else echo "<option value=\"$i\">$i</option>";?>
    </select>
    <select disabled name="month_actv" class="textbox">
    <?foreach ($month as $i => $m)
        if ($i == date("m")) echo "<option selected value=\"$i\">$m</option>";
                        else echo "<option value=\"$i\">$m</option>";?>
    </select>
    <select disabled name="day_actv" class="textbox">
    <?for($i=1;$i<32;$i++)
        if ($i == date("d")) echo "<option selected value=\"$i\">$i</option>";
                        else echo "<option value=\"$i\">$i</option>";?>
    </select></td>
</tr>
<tr><td width="25%">IP :</td>
    <td width="75%">
	<input class="textbox" name="framed_ip" size="16" maxlength="16">
<!--
	<input class="textbox" name="framed_ip4" size="1" maxlength="3"><b>.</b>
	<input class="textbox" name="framed_ip8" size="1" maxlength="3"><b>.</b>
	<input class="textbox" name="framed_ip16" size="1" maxlength="3"><b>.</b>
	<input class="textbox" name="framed_ip32" size="1" maxlength="3">
-->
	</td></tr>
<!--
<tr><td width="25%">Callback :</td>
    <td width="75%"><input class="textbox" name="callback" size="16" maxlength="16"></td></tr>
-->
<tr><td width="25%">mail :</td>
    <td width="75%"><input class="textbox" name="mail" size="20" maxlength="30"></td></tr>
</tbody>
</table>

<hr width="75%" size="1" noshade="" align="center">
<table width="75%" border="0" cellpadding="3" cellspacing="2" class="stxt">
<tbody>
<tr><td width="25%">Имя:</td>
    <td width="75%"><input name="rname" class="textbox" size="25" maxlength="60"></td>
</tr>
<tr><td width="25%">Телефон:</td>
    <td width="75%"><input name="phone" class="textbox" size="25" maxlength="60"></td>
</tr>
<tr><td width="25%">Адрес:</td>
    <td width="75%"><input name="address" class="textbox" size="45" maxlength="150"></td>
</tr>
<tr><td width="25%">Примечание:</td>
    <td width="75%"><textarea wrap="virtual" rows="4" cols="40" name="prim" class="textbox"></textarea></td>
</tr>
</tbody>
</table>
<hr width="75%" size="1" noshade="" align="center">
<input type="submit" value=" Добавить ">
<input type="reset" value=" Очистить " onclick="
   use_deposit.checked=true;
   deposit.disabled=true;
   credit.disabled=true;

   use_exp.checked=true;
   year_exp.disabled=true;
   month_exp.disabled=true;
   day_exp.disabled=true;
   hour_exp.disabled=true;
   min_exp.disabled=true;

   exp_credit.disabled=true;
   use_activate.checked=true;
   year_actv.disabled=true;
   month_actv.disabled=true;
   day_actv.disabled=true;

   ">
</form>
</center>
<? echo $end_head; exit; }
  ######################## end Form New User ############################
?>



<?  ######################## Test  ##################################
                if(isset($action) && $action == "test"){
?>

<br><center><b><font color="DARKRED">Создать / Удалить Тесты</font></b>
<hr width="45%">
<form action="<?echo "$PHP_SELF?action=test_action";?>" method="post">
<table border="0" width="20%" class="stxt"><tbody>
<tr><td align="right">
	<input type="radio" name="usetest" value="1" checked="checked"></td>
    <td> Создать</td></tr>
<tr><td align="right">
	<input type="radio" name="usetest" value="0"></td>
    <td> Удалить</td></tr>

<tr><td>Первый:</td>
    <td><input name="first" class="textbox" size="10" maxlength="5"></td></tr>
<tr><td>Последний:</td>
    <td><input name="last" class="textbox" size="10" maxlength="5"></td></tr>

</tbody></table>
<center><br><input type="submit" name="report" value="Создать / Удалить Тест"></center>
</form>
<hr width="45%">Всего в Базе <b>20</b> тестов. <a href="<?echo "$PHP_SELF?action=list_test";?>">Просмотреть.</a>
<hr width="45%"><font size="1">При вводе первого и последнего теста используются только цифры</font>
</center>

<? echo $end_head; exit; } ### end Test	?>


<?		# ---------------------------
		# 	Stat 

         if(isset($action) && ( $action == "stat" || $action == "user")){
?>
<center>
<form method="post" action="<?echo "$PHP_SELF?action=stat";?>">
<table>
<tr><td><select name="fmonth">
<?
 $query = "SELECT date_format(start_time,'%Y-%m') FROM usertime GROUP BY date_format(start_time,'%Y %m')";

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
<td><input type="submit" value="Показать"></td>
</table>
</form>

<center>
<table width="95%" border="0" cellpadding="3" cellspacing="2" align="center">
<tbody>

<tr class="stxt" bgcolor="#c8c8ff">
<th width="5%">P</th>
<th width="10%">Имя</th>
<th width="15%">Дата</th>
<th width="25%">Старт - Стоп</th>
<th width="18%">Время</th>
<th width="8%">InBytes</th>
<th width="8%">OutBytes</th>
<th width="8%">Стоимость</th>
<? if (!isset($user)){ ?>
<th width="10%">Остаток</th>
<? } ?>
<th width="10%">ip</th>
<th width="4%">port</th>
</tr>
<?
 if (isset($user)){
 $query = "SELECT name,date_format(start_time,'%d %b'),date_format(start_time,'%H:%i:%s'),time_on,date_format(stop_time,'%H:%i:%s'),inbytes,outbytes,round(cost,2),ip,port,deposit,prefix FROM usertime";
# $query .= " WHERE name='$user'";
 } else
 $query = "SELECT name,date_format(MAX(stop_time),'%d %b %H:%i:%s'),COUNT(time_on),SUM(time_on),SUM(inbytes),SUM(outbytes),round(SUM(round(cost,2)),2) FROM usertime";
# $query = "SELECT name,date_format(MAX(stop_time),'%d %b %H:%i:%s'),COUNT(time_on),SUM(time_on),SUM(inbytes),SUM(outbytes),SUM(cost) FROM usertime";
# if (isset($name) && $action == "user")
# $query .= " WHERE name='$name'";

  if (isset($fmonth))
 $query .= " WHERE date_format(start_time,'%Y-%m') = '$fmonth'";
  else{
 $query .= " WHERE date_format(start_time,'%Y-%m') = date_format(NOW(),'%Y-%m')";
 $fmonth = date("Y-m");
 }

  if (isset($user))
 $query .= " AND name='$user'";

  if (!isset($user))
# $query .= " ORDER BY stop_time";
# $query .= " GROUP BY name,date_format(start_time,'%Y-%m')";
 $query .= " GROUP BY name";
  else 
 $query .= " ORDER BY start_time";

# echo $query;
 $res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");

  $flagcolor = 0;
while(($row = mysql_fetch_array($res))){
	echo "<tr align=\"center\" class=\"stxt\"";
  if($flagcolor == 1){
	echo " bgcolor=\"#e1e1e1\""; $flagcolor = 0;
   } else {  $flagcolor = 1; }
	echo ">";

if (!isset($user)){
	$query = "SELECT start_time,stop_time,deposit FROM usertime WHERE name = '$row[0]' AND date_format(stop_time,'%Y-%m') = '$fmonth' ORDER BY stop_time DESC LIMIT 1";
	# echo $query;
 	$res_ost = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
	$row_ost = mysql_fetch_array($res_ost);
	$ost = round($row_ost[2], 2);

	$cost = round($row[6], 2);
	
	$time_on = sprintf("%02d %02d:%02d:%02d",
	 floor($row[3]/(3600*24)),floor($row[3]%(3600*24)/3600),floor(($row[3]%3600)/60),floor($row[3]%60));
echo	"<td>&nbsp;</td>
	 <td><a href=\"$PHP_SELF?action=user&name=$row[0]&use=info\"><b>$row[0]</b></a></td>
	 <td>$row[1]</td>
	 <td><a href=\"$PHP_SELF?action=stat&user=$row[0]&fmonth=$fmonth\">>> $row[2] <<</td>
	 <td>$time_on</td>
	 <td>".prts($row[4])."</td>
	 <td>".prts($row[5])."</td>
	 <td>$row[6]</td>
	 <td>$ost</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 </tr>";
}else{
	$time_on = sprintf("%02d %02d:%02d:%02d",
	 floor($row[3]/(3600*24)),floor($row[3]%(3600*24)/3600),floor(($row[3]%3600)/60),floor($row[3]%60));
echo	"<td>$row[11]</td>
	 <td><a href=\"$PHP_SELF?action=user&name=$row[0]&use=info\"><b>$row[0]</b></a></td>
	 <td>$row[1]</td>
	 <td>$row[2] -- $row[4]</td>
	 <td>$time_on</td>
	 <td>".prts($row[5])."</td>
	 <td>".prts($row[6])."</td>
	 <td>$row[7]</td>
	 <td>$row[8]</td>
	 <td>$row[9]</td>
	 </tr>";
}
} 

 $query = "SELECT COUNT(time_on),SUM(time_on),SUM(inbytes),SUM(outbytes),round(SUM(round(cost,2)),2) FROM usertime";
  if (isset($fmonth))
 $query .= " WHERE date_format(start_time,'%Y-%m') = '$fmonth'";
  else{
 $query .= " WHERE date_format(start_time,'%Y-%m') = date_format(NOW(),'%Y-%m')";
 $fmonth = date("Y-m");
 }

  if (isset($user))
 $query .= " AND name='$user'";
#  if (!isset($user))
# $query .= " GROUP BY name";
#  else 
# $query .= " GROUP BY start_time";

	$res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
	$row = mysql_fetch_array($res);

	$time_on = sprintf("%02d %02d:%02d:%02d",
	 floor($row[1]/(3600*24)),floor($row[1]%(3600*24)/3600),floor(($row[1]%3600)/60),floor($row[1]%60));

   echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#21ae91\">
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>$row[0]</td>
	 <td>$time_on</td>
	 <td>".prts($row[2])."</td>
	 <td>".prts($row[3])."</td>
	 <td>$row[4]</td>";
if (!isset($user)){ 
   echo "<td>&nbsp;</td>";
	}
   echo "<td>&nbsp;</td>
	 <td>&nbsp;</td>
	 </tr>";

?>
</tbody></table>
<br><br>
</center>

<? echo $end_head; exit; } ### end Stat		?>


<? ###################### Online user  ##############################
                if(isset($action) && ($action == "online" ||
				      $action == "delete") ){

	if($action == "delete"){
		mpdkill($port);
		for ($i = 0; $i < 600000; $i++);
	}
?>
<center>
<table width="95%" border="0" cellpadding="3" cellspacing="2" align="center">
<tbody>

<tr class="stxt" bgcolor="#c8c8ff">
<th width="2%" align="center">P</th>
<th width="8%" align="center">login</th>
<th width="30%">start</th>
<th width="23%">alive</th>
<th width="12%" align="center">ip</th>
<th width="2%">port</th>
<!--
<th width="15%">called-id</th>
-->
<th width="12%">caller-id</th>
<th width="8%">InBytes</th>
<th width="8%">OutBytes</th>
<th width="5%">cost</th>
<th width="2%">status</th>
<th width="5%"></th>
</tr>

<?

 $query  = "SELECT name,date_format(start_time,'%d %b %H:%i:%s'),date_format(stop_time,'%d %b %H:%i:%s'),ip,port,call_to,call_from,inbytes,outbytes,time_on,round(cost,2),status,prefix FROM usertime";
 $query .= " WHERE stop_time = '0000-00-00 00:00:00' OR ( status = 1 OR status = 3 ) GROUP BY start_time";
 $res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");

  $flagcolor = 0;
while(($row = mysql_fetch_array($res))){

#	if (isset($port) && $port == $row[4]) continue; ## hmmmm too harry

   echo "<tr align=\"center\" class=\"stxt\"";
  if($flagcolor == 1){
   echo " bgcolor=\"#e1e1e1\""; $flagcolor = 0;
   } else {  $flagcolor = 1; }
	echo ">";

	$time_on = sprintf("%02d %02d:%02d:%02d",
	 floor($row[9]/(3600*24)),floor($row[9]%(3600*24)/3600),floor(($row[9]%3600)/60),floor($row[9]%60));

	echo "<td align=\"center\">$row[12]</td>";	# prefix
	echo "<td><a href=\"$PHP_SELF?action=user&name=$row[0]&use=info\"><b>$row[0]</b></a></td>";
	echo "<td align=\"center\">$row[1]</td>";	# start
#	echo "<td align=\"center\">$row[2]</td>";	# stop
	echo "<td align=\"center\">$time_on</td>";	# time_on
	echo "<td align=\"center\">$row[3]</td>";	# ip
	echo "<td align=\"center\">$row[4]</td>";	# port
#	echo "<td align=\"center\">$row[5]</td>";	# call_to
	echo "<td align=\"center\">$row[6]</td>";	# call_from
	echo "<td align=\"center\">".prts($row[7])."</td>";	# in
	echo "<td align=\"center\">".prts($row[8])."</td>";	# out
	echo "<td align=\"center\">$row[10]</td>";	# cost
	echo "<td align=\"center\">$row[11]</td>";	# cost
	echo "<td align=\"center\"><a href=\"$PHP_SELF?action=delete&port=$row[4]\">удалить</a></td>";
	echo "</tr>";
}

 $query  = "SELECT SUM(inbytes),SUM(outbytes),SUM(time_on),round(SUM(round(cost,2)),2) FROM usertime";
 $query .= " WHERE stop_time = '0000-00-00 00:00:00' OR ( status = 1 OR status = 3 )";
 $res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
 $row = mysql_fetch_array($res);

   echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#21ae91\">
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 <td>".prts($row[0])."</td>
	 <td>".prts($row[1])."</td>
	 <td>$row[3]</td>
	 <td>&nbsp;</td>
	 <td>&nbsp;</td>
	 </tr>"

?>



</tbody></table>
<br><br>
</center>
<? echo $end_head; exit; }
  ######################## end online user #############################
?>



<?  ######################## system main menu #############################
        if(isset($action) && $action == "system" ){        ?>
<br><center>
<table width="35%" border="0" cellpadding="3" cellspacing="2">
<tbody>
<tr class="wtxt"><td><a href="<?echo "$PHP_SELF?action=downcostgroup";?>">Помегабайтные Скидки</a></td></tr>
<tr class="wtxt"><td><a href="<?echo "$PHP_SELF?action=hourgroup";?>">Почасовые Прайсы</a></td></tr>
<tr class="wtxt"><td><a href="<?echo "$PHP_SELF?action=trafgroup";?>">Помегабайтные Прайсы</a></td></tr>
<tr class="wtxt"><td><a href="<?echo "$PHP_SELF?action=hl";?>">Праздничные Дни</a></td></tr>
<tr class="wtxt"><td><a href="<?echo "$PHP_SELF?action=group";?>">Группы Доступа</a></td></tr>
<tr class="wtxt"><td><a href="<?echo "$PHP_SELF?action=oper";?>">Операторы Системы</a></td></tr>
<tr class="wtxt"><td><a href="<?echo "$PHP_SELF?action=backup";?>">MySQL DataBase</a></td></tr>
</tbody>
</table></center>
<? }
  ########################## end system menu #######################
?>




<?  ########################### Operator list all ######################
        if(isset($action) && $action == "oper" ){ 

   if(isset($use) && $use == "access"){
	$query = "UPDATE operators SET access='$access' WHERE name='$name'";
	$res = mysql_query("$query") or 
		die("Error query <font color=\"RED\">$query</font>");
    	$msg = "Изменен доступ на <b>$mode_oper[$access]</b> Оператору: <b>$name</b>";
    	history_log("- -",$msg);
	$use = "info";
   }

   if(isset($use) && $use == "delete"){
   if(isset($delete) && $delete == "1"){
	$query = "DELETE FROM operators WHERE name='$name'";
	$res = mysql_query("$query") or 
		die("Error query <font color=\"RED\">$query</font>");

    $msg = "Оператор: <b>$name</b> - <b>Удален</b>";
    history_log("- -",$msg);
    } 
     $use = "info";
   }

 if(isset($use) && $use == "chpass" && (isset($passwd) || isset($passwd2))){
	if ($passwd == $passwd2 && strlen($passwd) >= 6){
		$passwd = mysql_escape_string($passwd);
	$query = "UPDATE operators SET passwd=encrypt('$passwd') WHERE name='$name'";
	$res = mysql_query("$query") or 
		die("Error query <font color=\"RED\">$query</font>");
    $msg = "Изменен пароль Оператору: <b>$name</b>";
    history_log("- -",$msg);
     $use = "info";
	   } else {
     $use = "fpasswd";
	if ($passwd != $passwd2)
		$msg = "несовпадают";
	if (strlen($passwd) < 6)
		$msg .= " должен быть неменее 6 символов";
?>
<td valign="center" width="95%"><center><br><br><center><b><?echo $msg?></b></center></td></center>
<?
  	  }
	}

 if(isset($use) && $use == "add" && isset($name) && isset($passwd)){
  $query = "INSERT INTO operators (name,passwd,access,rname) VALUES ('$name',encrypt('$passwd'),'$access','$rname')";
  $res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
    $msg = "Добавлен новый Оператор: <b>$name</b>";
    history_log("- -",$msg);
	$use = "info";
 }

      if(!isset($use) || (isset($use) && $use=="info")){

 $query = "SELECT name,rname,access FROM operators ORDER BY name";
 $res = mysql_query("$query") or die("Error query <font color=\"RED\">$query</font>");
?>
<br><center><b>Список Операторов Биллинговой Системы </b><br><br>
<table width="80%" border="0" cellpadding="2" cellspacing="2">
<tbody>
<tr class="stxt" bgcolor="#c8c8ff">
    <th width="20%">Оператор</th>
    <th width="30%">Имя</th>
    <th width="20%">Права</th>
    <th width="15%">Пароль</th>
    <th width="15%"></th>
</tr>
<?  $flagcolor = 0;
while(($rowoper = mysql_fetch_array($res))){
	echo "<tr align=\"center\" class=\"stxt\"";
  if($flagcolor == 1){
	echo " bgcolor=\"#e1e1e1\">"; $flagcolor = 0;
   }else{
	echo ">";                     $flagcolor = 1;
 }

 echo "<td>$rowoper[0]</td>";
 echo "<td>$rowoper[1]</td>";
 $imode = $rowoper[2];
 echo "<td><a href=\"$PHP_SELF?action=oper&use=faccess&name=$rowoper[0]\"><b>$mode_oper[$imode]</b></a></td>";
?>
 <td><a href="<?echo "$PHP_SELF?action=oper&use=fpasswd&name=$rowoper[0]";?>">Cменить</a></td>
 <td><a href="<?echo "$PHP_SELF?action=oper&use=fdelete&name=$rowoper[0]";?>">Удалить</a></td>
</tr>
<?}?>

</tbody>
</table>
<br>
<form method="post" action="<?echo "$PHP_SELF?action=oper&use=fadd";?>">
<input type="submit" value="Добавить">
</form>
</center>
<? }
 ########################### end Operator list ####################
?>

<? ###########################  Operator fadd ####################
        if(isset($use) && $use=="fadd" ){     ?>
<form method="post" action="<?echo "$PHP_SELF?action=oper&use=add";?>">
<table border="0" width="55%" align="center" valign="top" cellpadding="2" cellspacing="2" class="stxt">
<tbody>
<tr><th bgcolor="#c8c8ff" colspan="2">Добавить Оператора:</th></tr>
<tr bgcolor="#e5e1ed">
    <td align="left">Логин</td>
    <td align="left"><input type="text" class="textbox" size="16" maxlength="32" name="name"></td>
</tr>
<tr bgcolor="#e5e1ed">
    <td align="left">Пароль:</td>
    <td align="left"><input type="password" class="textbox" size="16" maxlength="32" name="passwd"></td>
</tr>
<tr bgcolor="#e5e1ed">
    <td width="35%">Доступ:</td>
    <td><select name="access" class="textbox">
        <option value="0">Read/Only</option>
        <option value="1">Read/Write</option>
        <option value="2">Full Access</option>
        </select>
    </td>
</tr>
<tr bgcolor="#e5e1ed">
    <td align="left">Фамилия</td>
    <td align="left"><input type="text" class="textbox" size="40" name="rname"></td>
</tr>
<tr>
<td colspan="2" align="center"><br><input type="submit" value="Добавить оператора"></td>
</tr>
</tbody>
</table>
</form>
<? }
  ####################### end Operator fadd ####################
?>

<? ###########################  Operator faccess ####################
        if(isset($use) && $use=="faccess" ){ 	?>
<center><br>
<b>Изменение прав доступа Опреатора:<font color="DARKED"><?echo $name?></font></b><br><br>
<form action="<?echo "$PHPSELF?action=oper&use=access&name=$name";?>" method="post">
<input type="hidden">
<select name="access" class="textbox">
<option value="0">Read/Only</option>
<option value="1">Read/Write</option>
<option value="2">Full Access</option>
</select>
<br><br><input type="submit" value="Изменить"><br>
</form>
</center>
<? }
  ####################### end Operator faccess ####################
?>

<? ###########################  Operator fpasswd ####################
        if(isset($use) && $use=="fpasswd" ){     ?>
<center>
<form action="<?echo "$PHP_SELF?action=oper&use=chpass&name=$name";?>" method="post" name="main">
<table width="50%" border="0" cellspacing="0" cellpadding="2">
<tbody>
<tr><td colspan="2" align="center" class="wtxt">
<font color="darkgreen"><b>Смена Пароля Оператора <font color=DARKED><?echo $name;?></font></b></font>
<br><br></td></tr>
<tr><td class="stxt"><b>Новый пароль :</b></td>
	<td><input type="password" name="passwd" class="textbox" size="12"></td></tr>
<tr><td class="stxt"><b>Повторите пароль :</b></td>
	<td><input type="password" name="passwd2" class="textbox" size="12"></td></tr>
<tr><td colspan="2" align="center">
	<br><input type="submit" name="submit" value="Сменить"></td></tr>
</tbody></table></center>
<? }
  ####################### end Operator fpasswd ####################
?>

<? ###########################  Operator fdelete ####################
        if(isset($use) && $use=="fdelete" ){     ?>
<center><br><b>Оператор: <font color=DARKED><?echo $name;?></font></b><br><br>
<form action="<?echo "$PHP_SELF?action=oper&name=$name&use=delete"?>" method=post>
<input type=checkbox name=delete value=1> &nbsp;
<b><font color=DARKGREEN>Удалить</font></b>
<br><br><input type=submit name=report value=Изменить><br></form>
</center>
<? }
  ####################### end Operator fdelete ####################
?>

<? echo $end_head; exit; }
  ########################### end Operator  ########################
?>



<? ###########################  History  ####################
        if(isset($action) && $action=="history" ){
?>
<center>
<form method="post" action="<?echo "$PHP_SELF?action=history";?>">
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
<td><input type="submit" value="Показать"></td>
</table>
</form>

<?
 if (!isset($fmonth)) $fmonth = date("Y-m");

 $query = "SELECT name,date_format(date,'%d %H:%i:%s'),action,operator FROM history";
 $query .= " WHERE date_format(date,'%Y-%m') = '$fmonth'";
 $query .= " ORDER BY date";
# echo $query;
 $res = mysql_query("$query") or 
		die("Error query <font color=\"RED\">$query</font>");
?>
<br><center><b>History</b><br><br>
<table width="80%" border="0" cellpadding="2" cellspacing="2">
<tbody>
<tr class="stxt" bgcolor="#c8c8ff">
    <th width="13%">Имя</th>
    <th width="20%">Дата</th>
    <th width="55%">Действие</th>
    <th width="15%">Оператор</th>
</tr>
<?  $flagcolor = 0;
while(($row = mysql_fetch_array($res))){
   echo "<tr align=\"center\" class=\"stxt\"";
  if($flagcolor == 1){
   echo " bgcolor=\"#e1e1e1\">"; $flagcolor = 0;
   }else{
   echo ">";                     $flagcolor = 1;
 }

 echo "<td><a href=\"$PHP_SELF?action=user&name=$row[0]&use=info\"><b>$row[0]</b></a></td>";
 echo "<td>$row[1]</td>";
 echo "<td>$row[2]</td>";
 echo "<td>$row[3]</td>";
?>
</tr>
<?}?>
</tbody>
</table>
</center>
<? echo $end_head; exit; }
  ########################### end History  ########################
?>




<? ############################ Holiday #############################
        if(isset($action) && $action == "hl" ){
?>
<br>
<center>
<table>
<tbody><tr><td valign="top">
<table cellpadding="0" cellspacing="0" border="0" bgcolor="#000000">
<tbody><tr><td>
<table cellpadding="4" cellspacing="1" border="0" bgcolor="#ffffff">
<tbody>
<tr><th>Праздники</th></tr>
<?
	if(isset($use) && $use == "add"){
	 $hlday =date("m-d",mktime(0,0,0,$mons,$days,0));
	 $query = "INSERT INTO holiday VALUES('$hlday')";
	 $res = mysql_query("$query") or die("Error query $query");
	}

	if(isset($use) && $use == "del"){
	 $query = "DELETE FROM holiday WHERE hl='$delhl'";
	 $res = mysql_query("$query") or die("Error query $query");
	}

	 $query = "SELECT * FROM holiday GROUP BY hl";
	 $res = mysql_query("$query") or die("Error query $query");
?>
<form action="<?echo "$PHP_SELF?action=hl&use=del";?>" method="post">
<tr><td align="center">
     <select name="delhl" size="10">
<? while(($row = mysql_fetch_row($res))){
        $wday = substr($row[0],0,-3);
        $dday = substr($row[0],3);
      echo "<option value=\"$row[0]\">".$dday." ".$monthy[$wday]."</option>";
      }
?>   </select>
    </td>
</tr>
<tr><td align="center"><input type="submit" name="del" value=" Удалить ">
</td></tr>

</form>
</tbody>
</table></td></tr></tbody>
</table></td>

<td width="25" nowrap=""><br></td>
<td valign="top">

<!-- calendar begin -->
<?
	if(isset($m) && isset($y)){
	  $cmonth = date("m",mktime(0,0,0,$m,1,$y));
	  $cyear  = date("Y",mktime(0,0,0,$m,1,$y));
	}else{
	  $cmonth = date("m");
	  $cyear = date("Y");
	}

	$lmonth = $nmonth = $cmonth;
	$lmonth--; $nmonth++; 
	if($lmonth < 1) $lmonth =12; 
	if($nmonth >12) $nmonth =1;

	$lyear = $nyear = $cyear;
	if($cmonth ==  1) $lyear--; 
	if($cmonth == 12) $nyear++;
?>
<table cellpadding="2" cellspacing="0" border="1">
<tbody>

<tr><td colspan="7" bgcolor="#ccccdd">
    <table cellpadding="0" cellspacing="0" border="0" width="100%">
    <tbody>
    <tr>
       <th width="20"><a href="<?echo "$PHP_SELF?action=hl&m=$lmonth&y=$lyear";?>">&lt;&lt;</a></th>
       <th><font size="2"><?echo $month[$cmonth]." ".$cyear?></font></th>
       <th width="20"><a href="<?echo "$PHP_SELF?action=hl&m=$nmonth&y=$nyear";?>">&gt;&gt;</a></th>
    </tr>
    </tbody>
    </table>
</td></tr>

<tr>
<?for($i=1;$i<8;$i++){
     echo "<th width=\"22\" class=\"stxt\">";
 if($i==6 || $i==7) echo "<font color=\"RED\">$mdays[$i]</font></th>";
             else   echo "$mdays[$i]</th>";
}?>
</tr>

<?
$swday = date("w",mktime(0,0,0,$cmonth,1,$cyear));
if($swday == 0) $swday = 7; # corrected !
$day = 0;
for($y=0;$y<6;$y++){
 if($day >0 && !checkdate($cmonth,$day,$cyear))break;
echo "<tr>";
 for($x=1;$x<8;$x++){
  if($swday == ($x+$y) && $day == 0) $day = 1;
  if(checkdate($cmonth,$day,$cyear) && ($day >= 1 && $day <=31)){
       if($day <= 9) $cur ="$cyear-$cmonth-0$day";
               else  $cur ="$cyear-$cmonth-$day";
      if(date("Y-m-d") == $cur) echo "<td class=\"stxt\" bgcolor=\"#00f1f1\">";
   else
   if($x == 6 || $x == 7) echo "<td class=\"stxt\" bgcolor=\"#e1e1e1\">";
                  else  echo "<td class=\"stxt\">";
      echo "<a href=\"$PHP_SELF?action=hl&use=add&mons=$cmonth&days=$day\">$day</a></td>";
   $day++;
  }else   echo "<td><font size=\"2\">&nbsp;</font></td>";
 }
echo "</tr>";
}
?>
</tbody></table>
<!-- calendar end -->
</td></tr></tbody></table>
<br><font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
<? echo $end_head; exit; }
  ######################### end Holiday frame #####################
?>



<? ######################### Group frame #########################
           if(isset($action) && $action == "group"){

?>
<?  ########################### form Group EDIT #####################
        if(isset($use) && $use == "edit" ){

 $query = "SELECT prefix,grpname,gid,rname,type,traf_cost,type_traf,daycost,1mb FROM grp WHERE gid='$gid'";
 $res = mysql_query("$query") or die("Error query $query");
 $rowgrp = mysql_fetch_row($res);

 $blocked_type23="disabled"; $blocked_type4="disabled";

 if ($rowgrp[4] == 2 || $rowgrp[4] == 3) $blocked_type23="";
               else if ($rowgrp[4] == 4) $blocked_type4="";
?>

<form action="<?echo "$PHP_SELF?action=group&use=update";?>" method="post">
<center><br><b>Редактировать Группу</b><br><br>
<table width="50%" border="0" cellpadding="1">
<tbody>
<tr><td class="wtxt"> Префикс:</td>
        <td><input name="prefix" class="textbox" value="<?echo $rowgrp[0]?>" size="1" maxlength="1"></td></tr>
<!-- <tr><td class="wtxt"> Группа:</td>
        <td><input name="grpname" class="textbox" value="<?echo $rowgrp[1]?>" size="16"></td></tr>
        -->
<tr><td class="wtxt"> Группа:</td>
    <td><select name="grpname" class="textbox">
        <?foreach ($ar = radusers() as $iname => $gname)
              if ($gname == $rowgrp[1]) echo "<option selected value=\"$gname\">$gname</option>";
                                   else echo "<option value=\"$gname\">$gname</option>";?>
        </select>
    </td></tr>
<tr><td class="wtxt"> GID:</td>
        <td><input name="gid" readonly="1" class="textbox" value="<?echo $rowgrp[2]?>" size="5"></td></tr>
<tr><td class="wtxt"> Имя:</td>
        <td><input name="rname" class="textbox" value="<?echo $rowgrp[3]?>" size="16"></td></tr>
<tr><td class="wtxt"> Тип:</td>
        <td><select name="types" class="textbox"
         onclick="
            val = types.options[types.selectedIndex].value;
          if(val == 0 || val == 1){
           daycost.disabled=true;trafcost.disabled=true;
           traftype.disabled=true;
           onemb.disable=true;
          }else
          if(val == 2 || val == 3){
           onemb.disable=false;
           trafcost.disabled=false;
           traftype.disabled=false;daycost.disabled=true;
          }else
          if(val == 4){
           daycost.disabled=false;trafcost.disabled=true;
           traftype.disabled=true;
           onebm.disable=true;
          }">
     <?for($i=0;$i<5;$i++)
       if ($rowgrp[4] == $i) echo "<option selected value=\"$i\">$grptype[$i]</option>";
                      else   echo "<option value=\"$i\">$grptype[$i]</option>";
      ?>
        </select>
   </td>
</tr>
<tr><td class="wtxt"><hr size="1">1 Mb:</td>
    <td><hr size=1"><input <?echo $blocked_type23;?> name="onemb" value="<?echo $rowgrp[8]?>" class="textbox" size="8"></td></tr>

<tr><td class="wtxt"><hr size="1">Цена за 1 Мб:</td>
    <td><hr size="1"><input <?echo $blocked_type23;?> name="trafcost" value="<?echo $rowgrp[5]?>" class="textbox" size="4"></td></tr>
<!--
<tr><td class="wtxt"> Cкорость:</td>
        <td><input  <?echo $blocked_type23;?> name="speedtraf" value="<?echo $rowgrp[6]?>" class="textbox" size="6"></td></tr>
-->
<tr><td class="wtxt"> Тип Траффика:</td>
        <td><select <?echo $blocked_type23;?> name="traftype" class="textbox">
        <?for($i=0;$i<4;$i++)
         if ($rowgrp[6] == $i)
        echo "<option selected value=\"$i\">$grptraf_type[$i]</option>";
        else
        echo "<option value=\"$i\">$grptraf_type[$i]</option>";
        ?>
        </select>
    </td>
</tr>
<tr><td class="wtxt"><hr size="1">Стоимость Дня:</td>
    <td><hr size="1"><input <?echo $blocked_type4;?> name="daycost" value="<?echo $rowgrp[7]?>" class="textbox" size="6"></td>
</tr>
<tr><td colspan="2" align="center"><br><br><input type="submit" name="report" value="  Обновить "></td></tr>
</tbody>
</table>
<br><font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
</center>
<? echo $end_head; exit; }
  ########################### end form Group EDIT ####################
?>


<? ########################### form Group ADD #####################
        if(isset($use) && $use == "add" ){  ?>

<form action="<?echo $PHP_SELF?>?action=group&use=create" method="post">
<center><br><b>Создать Группу Доступа</b><br><br>
<table width="50%" border="0" cellpadding="1">
<tbody>
<tr><td class="wtxt"> Префикс:</td>
        <td><input name="prefix" class="textbox" size="3" maxlength="1"></td></tr>
<!-- <tr><td class="wtxt"> Группа:</td>
        <td><input name="grpname" class="textbox" size="16"></td></tr> -->
<tr><td class="wtxt"> Группа:</td>
    <td><select name="grpname" class="textbox">
        <?foreach ($ar = radusers() as $iname => $gname) echo "<option value=\"$gname\">$gname</option>";?>
        </select>
    </td></tr>
<tr><td class="wtxt"> GID:</td>
        <td><input name="gid" class="textbox" size="5"></td></tr>
<tr><td class="wtxt"> Имя:</td>
        <td><input name="rname" class="textbox" size="16"></td></tr>
<tr><td class="wtxt"> Тип:</td>
    <td><select name="types" class="textbox"
         onclick="
            val = types.options[types.selectedIndex].value;
          if(val == 0 || val == 1){
            daycost.disabled=true;trafcost.disabled=true;
            speedtraf.disabled=true;traftype.disabled=true;
            onemb.disabled=true;
          }else
          if(val == 2 || val == 3){
            onemb.disabled=false;
            trafcost.disabled=false;
            traftype.disabled=false;daycost.disabled=true;
          }else
          if(val == 4){
            daycost.disabled=false;trafcost.disabled=true;
            traftype.disabled=true;
            onemb.disabled=true;
          }">
        <?for($i=0;$i<5;$i++) echo "<option value=\"$i\">$grptype[$i]</option>";?>
        </select>
   </td>
</tr>
<tr><td class="wtxt"><hr size="1">1 Mb:</td>
    <td><hr size=1"><input disabled name="onemb" value="1048576" class="textbox" size="8"></td></tr>
<tr><td class="wtxt"><hr size="1">Цена за 1 Мб:</td>
    <td><hr size="1"><input disabled name="trafcost" value="0" class="textbox" size="4"></td></tr>
<!--
<tr><td class="wtxt"> Cкорость:</td>
        <td><input disabled name="speedtraf" value="0" class="textbox" size="6"></td></tr>
-->
<tr><td class="wtxt"> Тип Траффика:</td>
        <td><select disabled name="traftype" class="textbox">
        <?for($i=0;$i<4;$i++) echo "<option value=\"$i\">$grptraf_type[$i]</option>";?>
        </select>
    </td>
</tr>
<tr><td class="wtxt"><hr size="1">Стоимость Дня:</td>
    <td><hr size="1"><input disabled name="daycost" value="0" class="textbox" size="6"></td>
</tr>
<tr><td colspan="2" align="center"><br><br><input type="submit" name="report" value="  Создать "></td></tr>
</tbody>
</table>
<br><font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
</center>
<? echo $end_head; exit; }
  ########################### end form Group ADD ####################
?>

<br><center><font class="wtxt">Все Группы Доступа и их настройки</font><br><br>
<?
 if(isset($use) && $use == "update"){

# $query = "SELECT gid FROM grp WHERE gid='$gid' AND prefix='$prefix'";
 $query = "SELECT gid FROM grp WHERE prefix='$prefix'";
 $res = mysql_query("$query") or die("Error query $query");
 $row = mysql_fetch_row($res);
# echo "fetch $row[0] =? $gid";
 $num_prefix = mysql_num_rows($res);
  if($num_prefix == 0 || ($num_prefix == 1 && $row[0] == $gid)){
    $query = "UPDATE grp SET prefix='$prefix',grpname='$grpname',rname='$rname',type=$types";
  if($types == 2 || $types == 3)
    $query .= ",traf_cost=$trafcost,type_traf=$traftype,1mb=$onemb";
  if($types == 4)
    $query .=",daycost=$daycost";
    $query .=" WHERE gid='$gid'";

    $res = mysql_query("$query") or die("Error query $query");
  }else{
    echo "<center>Группа с префиксом <b>$prefix</b> уже есть</center><br>";
    $result = 2;
  }
 }

 if(isset($use) && $use == "create"){
 $query = "SELECT gid FROM grp WHERE gid='$gid' OR prefix='$prefix'";
 $res = mysql_query("$query") or die("Error query $query");
  if(mysql_num_rows($res) != 0){
    echo "<center><font class=\"stxt\">Группа с ( GID : <b>$gid</b> ) или префиксом <b>$prefix</b> уже есть</font></center><br>";
    $result = 1;
    }else{
      if($types == 0 || $types == 1 || $types == 2 || $types == 3) $daycost = 0;
      if($types == 0 || $types == 1 || $types == 4){ $trafcost= 0; $traftype= 0; }

    $query = "INSERT INTO grp (prefix,grpname,gid,rname,type,traf_cost,1mb,type_traf,daycost) VALUES ('$prefix','$grpname',$gid,'$rname',$types,$trafcost,$onemb,$traftype,$daycost)";
    $res = mysql_query("$query") or die("Error query $query");
    }
 }

 if(isset($use) && $use == "delete"){

  $query = "DELETE FROM grp WHERE gid='$gid'";
  $res = mysql_query("$query") or die("Error query $query");
 }

  $query = "SELECT prefix,grpname,gid,rname,type,traf_cost,type_traf,daycost,use_callback,1mb FROM grp ORDER BY gid";
  $res = mysql_query("$query") or die("Error query $query");
?>

<table width="97%" border="0" cellpadding="1" class="txt">
<tbody>
<tr bgcolor="#c8c8ff" class="stxt">
<th width="3%"></th>
<th width="14%">Группа</th>
<th width="8%">GID</th>
<th width="20%">Имя</th>
<th width="28%">Тип</th>
<th width="1%">CB</th>
<th width="35%">Дополнительно</th>
<th width="6%"></th>
</tr>
<?
 while(($row = mysql_fetch_row($res))){
  if(isset($result)  && $result == 1 &&
    ((isset($gid)    && $gid == $row[2]) ||
     (isset($prefix) && $prefix == $row[0])) )
       echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#e0b0b0\">";
  else
 if(isset($result)  && $result == 2 &&
    isset($prefix)  && $prefix == $row[0])
       echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#e0b0b0\">";
  else echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#e1e1e1\">";

  echo "<td><b>$row[0]</b></td>";
  echo "<td><a href=\"$PHP_SELF?action=group&use=edit&gid=$row[2]\"><b>$row[1]</b></a></td>";
  echo "<td>$row[2]</td>";
  echo "<td><b>$row[3]</b></td>";
  $igrptype = $row[4];
  $igrptype_traf = $row[6];

if ($igrptype == 1 || $igrptype == 2 ){
  $query = "SELECT * FROM hourgroup WHERE gid='$row[2]'";
  $reshour = mysql_query("$query") or die("Error query $query");
  $numhour = mysql_num_rows($reshour);
  echo "<td><b><a href=\"$PHP_SELF?action=hourgroup&use=view&gid=$row[2]\">";
  if($numhour == 0) echo "<font color=\"#ff0000\">$grptype[$igrptype] (нет!)";
               else echo "<font color=\"#3d54f0\">$grptype[$igrptype]";
   echo "</font></a><b></td>";
  }
  else
if (($igrptype == 2 || $igrptype == 3) && $row[5] == 0 ){

  $query = "SELECT * FROM trafgroup WHERE gid='$row[2]'";
  $reshour = mysql_query("$query") or die("Error query $query");
  $numhour = mysql_num_rows($reshour);
  echo "<td><b><a href=\"$PHP_SELF?action=trafgroup&use=view&gid=$row[2]\">";
  if($numhour == 0) echo "<font color=\"#ff0000\">$grptype[$igrptype] (нет!)";
               else echo "<font color=\"#3d54f0\">$grptype[$igrptype]";
   echo "</font></a><b></td>";
  }
  else
  echo "<td><b>$grptype[$igrptype]</b></td>";

  if($row[8] == 1)  echo "<td>yes</td>";
               else echo "<td></td>";

if ($igrptype == 2 || $igrptype == 3 ){

  echo "<td>( <b>$row[5]</b>/ <b>$grptraf_type[$igrptype_traf]</b> / <b>1Mb=$row[9]b</b>)</td>";
 }else
 if ($igrptype == 4)  echo "<td>Стоимость дня: <b>$row[7]</b></td>";
                else  echo "<td></td>";

  echo "<td><a href=\"$PHP_SELF?action=group&use=delete&gid=$row[2]\" onclick=\"return confirm('УДАЛИТЬ Группу: $row[2]')\"> Удалить </a></td>";
  echo "</tr>";
 }
?>
</tbody></table>

<form action="<?echo $PHP_SELF?>?action=group&use=add" method="post"><br>
<input type="submit" name="report" value="  Создать ">
</form>
<font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
</center>
<? echo $end_head; exit; }
  ######################### end Group frame ########################
?>

<?		#------------------------------------
		#  DownCostGroup frame 
		#

	if(isset($action) && $action == "downcostgroup"){
?>


<?   if (isset($use) && ($use == "view" || $use == "editadd" || $use == "editdel")){
	if ($use == "editadd"){
		if (!isset($cost)) $cost = 0;	
		if (is_numeric($bytes) && is_numeric($cost)){
            $query = "INSERT INTO tariff (gid,type,bytes,cost) VALUES ('$gid',0,'$bytes','$cost')";
            $res = mysql_query("$query") or die("Error query $query");
		}
	}

	if ($use == "editdel"){
            $query = "DELETE FROM tariff WHERE gid='$gid' AND bytes='$bytes'";
	    #echo $query;
            $res = mysql_query("$query") or die("Error query $query");
	}

        $query = "SELECT gid,type,bytes,cost FROM tariff WHERE gid='$gid' ORDER BY bytes";
	#echo $query;
        $res = mysql_query("$query") or die("Error query $query");
#        $rw = mysql_fetch_array($res);
        $row = mysql_fetch_row($res);
	$ncount = mysql_num_rows($res);
	if ($ncount > 0){
?>
<center> Группа: <b>
	<? foreach($ggid_list as $gidn => $grname)if ($row[0] == $gidn) echo $grname;?>
	</b></center><br>
<center>
<table width="95%" border="0" cellpadding="1">
<tbody>
<tr class="stxt" bgcolor="#c8c8ff">
</tr>
<td align="center" class="wtxt" width="5%">
<form action="<?echo "$PHP_SELF?action=downcostgroup&use=editdel&gid=$gid";?>" method=post>
<select name="bytes" class="textbox" size="10">
<?
#	while (($row = mysql_fetch_row($res)))
	do {
		$cost = $row[3];
		$bytes = $row[2];

		if ($cost == 0){
			$tables = sprintf("%20s : Прайс", prts($bytes));
		} else {
			$tables = sprintf("%20s : %.4f", prts($bytes), $cost);
		}
#		$tables = sprintf("%.4f : %-20s", $cost, prts($bytes));
#		$tables = sprintf("%.4f : %20d", $cost, $bytes);
#		echo "<option value=\"\">$row[2] : $row[3]</option>";		
#		echo "<option value=\"$bytes\">".sprintf("%20s", $bytes)." : ".$cost."</option>";		
		echo "<option value=\"$bytes\">".$tables."</option>";		
	} while (($row = mysql_fetch_row($res)));
?>
</select>
<br><br><input type=submit name=report value=Удалить><br></form>
</td>

<td align="center" class="wtxt" width="5%">
<form action="<?echo "$PHP_SELF?action=downcostgroup&use=editadd&gid=$gid";?>" method=post>
<table>
<tr>
<td>bytes:<input name="bytes" class="textbox" size="20"></td>
<td>price:<input name="price" type="checkbox" value="1" onclick=" if (price.checked == true){ cost.disabled = true; cost.value=0;} else { cost.disabled = false; cost.value = ''; } "></td>
<td>cost:<input name="cost" class="textbox" size="5"></td>
</tr>
</table>
<br><br><input type=submit name=report value=Добавить><br></form>
</td>

</tbody>
</table>

<center><a href="<?echo "$PHP_SELF?action=downcostgroup";?>"><b>Список Скидок по группам</b></a>
<center><br><font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
<? 
    echo $end_head; exit; }
	unset($use);
}
  ################### end frame downcost VIEW or EDIT ####################
?>

<?
  if ((isset($use) && ($use == "delete" || $use == "add")) || !isset($use)){

        if (isset($use) && $use == "delete"){
            $query = "DELETE FROM tariff WHERE gid='$gid'";
            $res = mysql_query("$query") or die("Error query $query");
         }

        if (isset($use) && $use == "add"){
		if (!isset($cost)) $cost = 0;	
		if (is_numeric($bytes) && is_numeric($cost)){
            #$query = "INSERT INTO tariff (gid,type,bytes,cost) VALUES('$gid',0,0,0)";
            $query = "INSERT INTO tariff (gid,type,bytes,cost) VALUES ('$gid',0,'$bytes','$cost')";
            $res = mysql_query("$query") or die("Error query $query");
		}
         }


        $query = "SELECT gid,type,bytes,cost,COUNT(gid) FROM tariff GROUP BY gid";
        $res = mysql_query("$query") or die("Error query $query");
?>
<br><center>
<table width="60%" border="0" cellpadding="3" cellspacing="0">
<tbody><tr bgcolor="#c8c8ff" class="stxt">
<?
   while (($row = mysql_fetch_row($res))){
   if (isset($result) && $result == 1 && $row[0] == $gid)
           echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#e0b0b0\">";
      else echo "<tr align=\"center\" class=\"stxt\">";
	$gidname = $ggid_list[$row[0]];
	$count = $row[4];
      echo "<td><center>Группа: ";
#      echo "<a href=$PHP_SELF?action=hourgroup&use=view&gid=$row[0]><b>hourgroup</b></a>";
      echo "<a href=$PHP_SELF?action=downcostgroup&use=view&gid=$row[0]><b>$gidname</b> [<font color=#ff001f>$count</font>]:downcostgroup</a>";
      echo " ( GID: <b>$row[0]</b>) ";
      echo "<a HREF=$PHP_SELF?action=downcostgroup&use=delete&gid=$row[0] onclick=\"return confirm('УДАЛИТЬ Группу: $gidname')\">Удалить</a><br>";
      echo "</center></td></tr>";
 } ?>
</tr></tbody>
</table>
</center>

<br>
<center>
<form action="<?echo "$PHP_SELF?action=downcostgroup&use=add";?>" method="post">
<tr>
<!-- <td><b>GID:</b><input name="gid" class="textbox" size="5"></td> -->
<td><select name="gid" class="textbox">
	<?foreach ($ggid_list as $igid => $iname)
			echo "<option value=\"$igid\">$iname ($igid)</option>";
	?></select>
</td>
<td>bytes:<input name="bytes" class="textbox" size="20"></td>
<td>price:<input name="price" type="checkbox" value="1" onclick=" if (price.checked == true){ cost.disabled = true; cost.value=0;} else { cost.disabled = false; cost.value = ''; } "></td>
<td>cost:<input name="cost" class="textbox" size="5"></td>
<td>&nbsp;<input type="submit" name="report" value=" Добавить "></td>
</tr>
</form>
</center>
<center><br><font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
<? echo $end_head;  exit;  }
 #################### end frame downcost DELETE or ADD ###################
?>

<? echo $end_head; exit;  }
  ######################### end Downcost frame ########################
?>

<?		#------------------------------------
		#  Hourgroup frame 
		#

       if(isset($action) && $action == "hourgroup"){

  if ((isset($use) && ($use == "delete" || $use == "add")) || !isset($use)){

        if (isset($use) && $use == "delete"){
            $query = "DELETE FROM hourgroup WHERE gid='$gid'";
            $res = mysql_query("$query") or die("Error query $query");
         }

        if (isset($use) && $use == "add"){
            $query = "SELECT gid FROM hourgroup WHERE gid='$gid'";
            $res = mysql_query("$query") or die("Error query $query");
            $num = mysql_num_rows($res);
          if($num != 0){
              echo "<center>Такая группа уже существует !!!</center>";
              $result = 1;
          }else
           for ($w=0;$w <= 7;$w++){
                $query = "INSERT INTO hourgroup VALUES (";
                for ($i=0;$i<24;$i++) $query .= "0,";
               # $query .= "$w".","."$gid".","."'Почасовая')";
                $query .= "$w".","."$gid".")";
                $res = mysql_query("$query") or die("Error query $query");
           }
       }

        $query = "SELECT gid FROM hourgroup GROUP BY gid";
        $res = mysql_query("$query") or die("Error query $query");
?>
<br><center>
<table width="50%" border="0" cellpadding="3" cellspacing="0">
<tbody><tr bgcolor="#c8c8ff" class="stxt">
<?
   while (($row = mysql_fetch_row($res))){
   if (isset($result) && $result == 1 && $row[0] == $gid)
           echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#e0b0b0\">";
      else echo "<tr align=\"center\" class=\"stxt\">";
	$gidname = $ggid_list[$row[0]];
      echo "<td><center>Группа: ";
#      echo "<a href=$PHP_SELF?action=hourgroup&use=view&gid=$row[0]><b>hourgroup</b></a>";
      echo "<a href=$PHP_SELF?action=hourgroup&use=view&gid=$row[0]><b>$gidname</b>:hourgroup</a>";
      echo " ( GID: <b>$row[0]</b>) ";
      echo "<a HREF=$PHP_SELF?action=hourgroup&use=delete&gid=$row[0] onclick=\"return confirm('УДАЛИТЬ Группу: $gidname')\">Удалить</a><br>";
      echo "</center></td></tr>";
 } ?>
</tr></tbody>
</table>
</center>

<br>
<center>
<form action="<?echo "$PHP_SELF?action=hourgroup&use=add";?>" method="post">
<!-- <b>GID:</b> <input name="gid" class="textbox" size="5"> -->
<td><select name="gid" class="textbox">
	<?foreach ($ggid_list as $igid => $iname)
			echo "<option value=\"$igid\">$iname ($igid)</option>";
	?></select>
</td>
<input type="submit" name="report" value=" Создать ">
</form>
</center>
<center><br><font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
<? echo $end_head;  exit;  }
 #################### end frame hourgroup DELETE or ADD ###################
?>

<?   if (isset($use) && ($use == "view" || $use == "edit")){

        if ($use == "edit"){
                $query = "UPDATE hourgroup SET";
                for ($i=$hourstart; $i <= $hourstop; $i++){
                       $query .= " h".$i."=".$cost;
                     if ($i!=$hourstop) $query .= ",";
                }
                $query .= " WHERE week=".$weeks." AND gid=".$gid;
                $res = mysql_query("$query") or die("Error query $query");
        }

        $query = "SELECT *  FROM hourgroup WHERE gid='$gid' GROUP BY week";
        $res = mysql_query("$query") or die("Error query $query");
        $rw = mysql_fetch_array($res);
        if(mysql_num_rows($res) != 0){
?>
<center>Почасовая Группа: <? echo "<b>".$ggid_list[$rw[25]]."</b> (GID: <b>".$rw[25]."</b> )"; ?></center><br>
<center>
<table width="95%" border="0" cellpadding="1">
<tbody>
<tr class="stxt" bgcolor="#c8c8ff">
<!-- <th>&nbsp;</th> -->
<th width="15%">день \ час</th>
<?for ($i=0; $i<24; $i++) echo "<th>$i</th>";?>
</tr>
<?      for ($i=0; $i<8;  $i++){
                echo "<tr class=\"stxt\" align=\"center\" bgcolor=\"#e1e1e1\">";
                echo "<td>$wdays[$i]</td>";
        for ($s=0; $s<24; $s++)
                echo "<td width=\"25\" bgcolor=\"\">".$rw[$s]."</td>";
                echo "</tr>";
        $rw = mysql_fetch_array($res);
        }
?>
</tbody>
</table>

<center>
<form action="<?echo "$PHP_SELF?action=hourgroup&use=edit&gid=$gid";?>" method="post">
Часы: <select name="hourstart" class="textbox">
<?  for ($i=0;$i<24;$i++)
     if ($i==0) echo "<option selected value=\"$i\">$i</option>";
          else  echo "<option value=\"$i\">$i</option>";
?>
</select> - <select name="hourstop" class="textbox">
<?  for ($i=0;$i<24;$i++)
      if($i==23) echo "<option selected value=\"$i\">$i</option>\n";
          else   echo "<option value=\"$i\">$i</option>\n";
?>
</select> <select name="weeks" class="textbox">
<?  for ($i=0;$i<8;$i++)
      if ($i==0) echo "<option selected value=\"$i\">$wdays[$i]</option>\n";
           else  echo "<option value=\"$i\">$wdays[$i]</option>\n";
?>
</select>
<input name="cost" class="textbox" value="0.0" size="6" maxlength="6">
<input type="submit" name="report" value=" Изменить ">
</form>
<?}else echo "<font color=\"RED\"><b>Такой группы ненайдено !</b></font><br><br>"; ?>
<center><a href="<?echo "$PHP_SELF?action=hourgroup";?>"><b>Список Почасовых групп</b></a>
<br><font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
<? echo $end_head; exit; }
  ################### end frame hourgroup VIEW or EDIT ####################
?>

<? echo $end_head; exit;  }
  ######################### end Hourgroup frame ########################
?>


<?		#------------------------------------
		#  Trafgroup frame 
		#

       if(isset($action) && $action == "trafgroup"){

  if ((isset($use) && ($use == "delete" || $use == "add")) || !isset($use)){

        if (isset($use) && $use == "delete"){
            $query = "DELETE FROM trafgroup WHERE gid='$gid'";
            $res = mysql_query("$query") or die("Error query $query");
         }

        if (isset($use) && $use == "add"){
            $query = "SELECT gid FROM trafgroup WHERE gid='$gid'";
            $res = mysql_query("$query") or die("Error query $query");
            $num = mysql_num_rows($res);
          if($num != 0){
              echo "<center>Такая группа уже существует !!!</center>";
              $result = 1;
          }else
           for ($w=0;$w <= 7;$w++){
                $query = "INSERT INTO trafgroup VALUES (";
                for ($i=0;$i<24;$i++) $query .= "0,";
               # $query .= "$w".","."$gid".","."'Почасовая')";
                $query .= "$w".","."$gid".")";
                $res = mysql_query("$query") or die("Error query $query");
           }
       }

        $query = "SELECT gid FROM trafgroup GROUP BY gid";
        $res = mysql_query("$query") or die("Error query $query");
?>
<br><center><table width="50%" border="0" cellpadding="3" cellspacing="0">
<tbody><tr bgcolor="#c8c8ff" class="stxt">
<?
   while (($row = mysql_fetch_row($res))){
   if (isset($result) && $result == 1 && $row[0] == $gid)
           echo "<tr align=\"center\" class=\"stxt\" bgcolor=\"#e0b0b0\">";
      else echo "<tr align=\"center\" class=\"stxt\">";
	$gidname = $ggid_list[$row[0]];
      echo "<td><center>Группа: ";
#      echo "<a href=$PHP_SELF?action=trafgroup&use=view&gid=$row[0]><b>trafgroup</b></a>";
      echo "<a href=$PHP_SELF?action=trafgroup&use=view&gid=$row[0]><b>$gidname</b>:trafgroup</a>";
      echo " ( GID: <b>$row[0]</b>) ";
      echo "<a HREF=$PHP_SELF?action=trafgroup&use=delete&gid=$row[0] onclick=\"return confirm('УДАЛИТЬ Группу: $gidname')\">Удалить</a><br>";
      echo "</center></td></tr>";
 } ?>
</tr></tbody>
</table>
</center>

<br>
<center>
<form action="<?echo "$PHP_SELF?action=trafgroup&use=add";?>" method="post">
<!-- <b>GID:</b> <input name="gid" class="textbox" size="5"> -->
<td><select name="gid" class="textbox">
	<?foreach ($ggid_list as $igid => $iname)
			echo "<option value=\"$igid\">$iname ($igid)</option>";
	?></select>
</td>
<input type="submit" name="report" value=" Создать ">
</form>
</center>
<center><br><font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
<?  echo $end_head;  exit;  }
 #################### end frame trafgroup DELETE or ADD ###################
?>

<?   if (isset($use) && ($use == "view" || $use == "edit")){

        if ($use == "edit"){
                $query = "UPDATE trafgroup SET";
                for ($i=$hourstart; $i <= $hourstop; $i++){
                       $query .= " h".$i."=".$cost;
                     if ($i!=$hourstop) $query .= ",";
                }
                $query .= " WHERE week=".$weeks." AND gid=".$gid;
                $res = mysql_query("$query") or die("Error query $query");
        }

        $query = "SELECT *  FROM trafgroup WHERE gid='$gid' GROUP BY week";
        $res = mysql_query("$query") or die("Error query $query");
        $rw = mysql_fetch_array($res);
        if(mysql_num_rows($res) != 0){
?>
<center>Помегабайтная Группа: <? echo "<b>".$ggid_list[$rw[25]]."</b> (GID: <b>".$rw[25]."</b> )"; ?></center><br>
<center>
<table width="95%" border="0" cellpadding="1">
<tbody>
<tr class="stxt" bgcolor="#c8c8ff">
<!-- <th>&nbsp;</th> -->
<th width="15%">день \ час</th>
<?for ($i=0; $i<24; $i++) echo "<th>$i</th>";?>
</tr>
<?      for ($i=0; $i<8;  $i++){
                echo "<tr class=\"stxt\" align=\"center\" bgcolor=\"#e1e1e1\">";
                echo "<td>$wdays[$i]</td>";
        for ($s=0; $s<24; $s++)
                echo "<td width=\"25\" bgcolor=\"\">".$rw[$s]."</td>";
                echo "</tr>";
        $rw = mysql_fetch_array($res);
        }
?>
</tbody>
</table>

<center>
<form action="<?echo "$PHP_SELF?action=trafgroup&use=edit&gid=$gid";?>" method="post">
Часы: <select name="hourstart" class="textbox">
<?  for ($i=0;$i<24;$i++)
     if ($i==0) echo "<option selected value=\"$i\">$i</option>";
          else  echo "<option value=\"$i\">$i</option>";
?>
</select> - <select name="hourstop" class="textbox">
<?  for ($i=0;$i<24;$i++)
      if($i==23) echo "<option selected value=\"$i\">$i</option>\n";
          else   echo "<option value=\"$i\">$i</option>\n";
?>
</select> <select name="weeks" class="textbox">
<?  for ($i=0;$i<8;$i++)
      if ($i==0) echo "<option selected value=\"$i\">$wdays[$i]</option>\n";
           else  echo "<option value=\"$i\">$wdays[$i]</option>\n";
?>
</select>
<input name="cost" class="textbox" value="0.0" size="6" maxlength="6">
<input type="submit" name="report" value=" Изменить ">
</form>
<?}else echo "<font color=\"RED\"><b>Такой группы ненайдено !</b></font><br><br>"; ?>
<center><a href="<?echo "$PHP_SELF?action=trafgroup";?>"><b>Список Помегабайтных групп</b></a>
</center>
<center><br><font class="stxt">При любых измениях данных, требуется рестарт ( killall -1 radiusd )</font>
<? echo $end_head; exit; }
  ################### end frame trafgroup VIEW or EDIT ####################
?>

<? echo $end_head; exit;  }
  ######################### end Trafgroup frame ########################
?>


<? ######################### Backup frame #########################
       if(isset($action) && $action == "backup"){
?>
<center><b><?echo $dbase;?></b</center>
<table widht="70%" border="0" cellspacing="2" cellpadding="4" class="txt">
<tbody>
<tr bgcolor="#c8c8ff" class="stxt">
<th>Таблица</th>
<th>Строк</th>
<th>Размер</th>
</tr>

<?
 $flagcolor = 0;
 $res = mysql_list_tables("$dbase");
for ($i = 0; $i < mysql_num_rows($res); $i++) {
   $tb_names[$i] = mysql_tablename($res, $i);
   $resq = mysql_query("SELECT * from $tb_names[$i]") or die("Error query $query");
   $numr = mysql_num_rows($resq);
   $numf = 0; #$numr * mysql_num_fields($resq);
   echo "<tr align=\"center\" class=\"stxt\"";
  if($flagcolor == 1){
   echo " bgcolor=\"#e1e1e1\">"; $flagcolor = 0;
   }else{
   echo ">";                     $flagcolor = 1;
  }
   echo "<td>$tb_names[$i]</td> <td>$numr</td> <td>$numf</td>";?>
</tr>
<?
}
?>
</tdbody>
</table>
</center>
<? echo $end_head; exit;  }
  ######################### end backup frame ########################
?>


<? ######################### Card frame #########################
       if(isset($action) && $action == "card"){
?>

<? echo $end_head; exit;  }
  ######################### end card frame ########################
?>

<? echo $end_head; exit; ?>
