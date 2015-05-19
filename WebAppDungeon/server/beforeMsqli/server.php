<?php
	$app_IP = '127.0.0.1';
	$listen_port = 6789;

	$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
	//reuse port
	socket_set_option($socket, SOL_SOCKET, SO_REUSEADDR, 1);
	socket_bind($socket, $app_IP, $listen_port);
	socket_listen($socket);

	$clients = array($socket);
	$user_input;
	$error;
	//include_file "push.php";

	//DB setup
	$db_hostname = "127.0.0.1";//"localhost";
	$db_user = "dungeon_user";
	$db_pw = "dungeon_user";
	$db_name = "dungeon_schema";//"Dungeon";
	$user_table = "Users";
	$map_table = "Map";
	$db_port = 3306;
	$user_col = "userID";
	$room_col = "roomID";
	$user_tbl_rows = " (userID, x, y, z) ";
	
	/*result of command to send back to client; 
	 *e.g. res:"success", cmd:"move", msg:"loc=0,0,1"
	 *e.g. res:"success", cmd:"yell", msg:"hello world"
	 *e.g. res:"error", cmd:"", msg:"invalid command"
	 */
	$result = array("res" => "", "cmd" => "", "msg" => "");

	function set_result($res, $cmd, $msg) {
		global $result;
		$result["res"] = $res;
		$result["cmd"] = $cmd;
		$result["msg"] = $msg;
	}
	
	//validate the command portion of the function, return cmd and arg in associative array
	function validate_cmd($input) {
		$valid_commands = ["move", "say", "yell", "list"];
		
		$has_arg = false;
		$arg = "";
		
		//empty string?
		if(strlen($input) == 0) {
			set_result("error", "", "Please enter a command");
			return false;
		}
		
		//only one word entered?
		$separator = strpos($input, " ");
		if(!$separator) {
			$separator = strlen($input);
		}
		else {
			$has_arg = true;
		}
		
		//is the first word a valid command?
		$cmd = strtolower(substr($input, 0, $separator));
		if(in_array($cmd, $valid_commands) == false) {
			set_result("error", "", "Invalid command");
			return false;
		}
		
		//command valid, has argument; store command and argument for later use
		if($has_arg) {
			$arg = trim(substr($input, $separator, strlen($input)));
		}
		
		return array("cmd" => $cmd, "arg" => $arg);
	}
	
	//command previously validated; argument valid?
	function validate_arg($input) {
		$valid_move_args = ["north", "south", "east", "west", "up", "down"];
		
		$cmd = $input["cmd"];
		$arg = $input["arg"];
		
		//command = move
		if($cmd == "move") {
			if($arg == "") {
				set_result("error", "", "In what direction?");
				return false;
			}
			//preserving case will not matter for move
			$user_input["arg"] = strtolower($arg);
			$arg = $user_input["arg"];
			if(in_array($arg, $valid_move_args)) {
				return true;
			}
			set_result("error", "", "Invalid argument");
			return false;
		}
		
		//command = list
		if($cmd == "list") {
			if($arg != "") {
				set_result("error", "", "Only type 'list'");
				return false;
			}
		}
		
		return true;
	}

	function setup_db_connection() {
		if (!$dbo) { die('mysqli_init failed'); }
		if (!$dbo->options(MYSQLI_INIT_COMMAND, 'SET AUTOCOMMIT = 1')) { die('Setting MYSQLI_INIT_COMMAND failed'); }
		if (!$dbo->options(MYSQLI_OPT_CONNECT_TIMEOUT, 5)) { die('Setting MYSQLI_OPT_CONNECT_TIMEOUT failed'); }
		
		if (!$dbo->real_connect($GLOBALS["db_hostname"], 
								$GLOBALS["db_user"], 
								$GLOBALS["db_pw"], 
								$GLOBALS["db_name"], 
								$GLOBALS["db_port"])) {
			//die('Connect Error (' . mysqli_connect_errno() . ') ' . mysqli_connect_error());
			set_result("error", "insert", "Database connection failed. Please try again later.");
			return false;
		}
		return true;
	}

	//when a user leaves the site, remove his/her data from the database
	function remove_user($user) {
		$dbo = mysqli_init();
		if(!setup_db_connection($dbo)) {
			return;
		}
		
		$user_row = "DELETE FROM " . $GLOBALS["user_table"] . " WHERE " . $GLOBALS["user_col"] . "='" . $user . "';";
		echo "user:$user removed from DB\n";
		$dbo->close();
	}

	function get_user_loc($db_con, $user) {
		$x_offset = 1;
		$y_offset = 2;
		$z_offset = 3;
		
		$curr_coord_q = "SELECT * FROM " . $GLOBALS["user_table"] . " WHERE " . $GLOBALS["user_col"] . "='" . $user . "';";
		//echo $curr_coord_q;
		$res = $db_con->query($curr_coord_q);
		$res->data_seek(0);
		$datarow = $res->fetch_array();
		
		$location = array("x"=>$datarow[$x_offset], "y"=>$datarow[$y_offset], "z"=>$datarow[$z_offset]);
		
		return $location;
	}

	function get_room_info($dbo, $room) {
		$trans_offset = 4;
		
		$query = "SELECT * FROM " . $GLOBALS["map_table"] . " WHERE " . $GLOBALS["room_col"] . "='" . $room . "';";
		$res = $dbo->query($query);
		$room_info;
		if($res->num_rows == 0) {
			$room_info = array("trans"=>"False");
		}
		else {
			$res->data_seek(0);
			$datarow = $res->fetch_array();

			//format nicely
			$room_info = array("trans"=>$datarow[$trans_offset]);
		}
		
		$dbo->close();
		return $room_info;
	}

	//user wants to move; attempt to store new location (coordinates given are relative), first checking if requested move is valid
	//using relative coordinates makes the job a little less clean, but allows for a single DB connection = more scalable
	function update_user_loc($user, $coord, $val) {
		$dbo = mysqli_init();
		if(!setup_db_connection($dbo)) {
			return;
		}
		
		//get current and new coordinates
		$curr_loc = get_user_loc($dbo, $user);
		$old_val = $curr_loc[$coord];
		$new_val = $old_val + $val;
		$requested_loc = $curr_loc;
		$requested_loc[$coord] = $new_val;
		$requested_room = "" . $requested_loc["x"];
		$requested_room .= $requested_loc["y"];
		$requested_room .= $requested_loc["z"];
		//echo $requested_room . "\n";
		
		//get info about room (check if transparent)
		$room_info = get_room_info($dbo, $requested_room);
		
		if($room_info["trans"] == "False") {
    		set_result("error", "insert", "You cannot move in that direction");
		}
		else {
			//update database
			$query = "UPDATE " . $GLOBALS["user_table"] . " SET " . $coord . "=" . $new_val . " WHERE " . $GLOBALS["user_col"] . "='" . $user . "';";
			if($dbo->query($query) == true) {
				$loc_msg = "loc:x=" . $requested_loc["x"] . ";y=" . $requested_loc["y"]  . ";z=" . $requested_loc["z"];
				set_result("success", "insert", $loc_msg);
			}
			else {
				set_result("error", "insert", "Update database error");
			}
		}
		
		$dbo->close();
	}

	function add_new_user($user) {
		$x = $y = $z = 0;
		
		$dbo = mysqli_init();
		if(!setup_db_connection($dbo)) {
			return;
		}
		
		$query = "INSERT INTO " . $GLOBALS["user_table"] . $GLOBALS["user_tbl_rows"] . " VALUES ('" 
			. $user . "', " 
			. $x . ", " 
			. $y . ", " 
			. $z . ");";
		
		if($dbo->query($query) == true) {
    		set_result("success", "insert", "Welcome");
		}
		else {
			set_result("error", "insert", "Username taken");
		}

		$dbo->close();
	}

	//process a move command
	function process_move($user, $input) {
		$cmd = $input["cmd"];
		$arg = $input["arg"];
		
		$col_to_update = "";
		$x = $y = $z = 0;
		
		$valid_args = array("north"=>1, "south"=>-1, "east"=>1, "west"=>-1, "up"=>1, "down"=>-1);
		$val_to_insert = $valid_args[$arg];
		if($arg == "north" || $arg == "south") {
			$col_to_update = "x";
		}
		else if($arg == "east" || $arg == "west") {
			$col_to_update = "y";
		}
		else if($arg == "up" || $arg == "down") {
			$col_to_update = "z";
		}
		
		if($col_to_update != "") {
			update_user_loc($user, $col_to_update, $val_to_insert);
		}
	}

	function get_users_by_loc($dbo, $loc) {
		if($loc === "all") {
			
		}
		else {
			
		}
	}

	//process a say or yell
	function process_msg($user, $input) {
		$arg = $input["arg"];
		$cmd = $input["cmd"];
		
		$dbo = mysqli_init();
		if(!setup_db_connection($dbo)) {
			return;
		}
		
		$users = array();
		
		if($cmd == "yell") {
			$arg = strtoupper($arg);
			get_users_by_loc($dbo, "all");
		}
		else {
			$location = get_user_loc($dbo, $user);
			get_users_by_loc($dbo, $location);
		}
		
		if(sizeof($users) > 0) {
			send_msg($users, $arg, $dbo);
		}
		
		set_result("success", $cmd, $arg);
		
		$dbo->close();
	}

	//process a list command
	function process_list($user, $input) {
		$cmd = $input["cmd"];
		
		$dbo = mysqli_init();
		if(!setup_db_connection($dbo)) {
			return;
		}
		$requested_loc = get_user_loc($dbo, $user);
		
		$requested_room = "" . $requested_loc["x"];
		$requested_room .= $requested_loc["y"];
		$requested_room .= $requested_loc["z"];
		
		$query = "SELECT * FROM " . $GLOBALS["user_table"] . " WHERE " . 
			"x=" . $requested_loc["x"] . " AND y=" . $requested_loc["y"] . " AND z=" . $requested_loc["z"] . ";";
		$res = $dbo->query($query);
		$num_rows = $res->num_rows;
		if($num_rows == 0) {
			set_result("success", $cmd, "empty");
		}
		else {
			$user_offest = 0;
			$separator = ", ";
			$msg = "";
			for($i = 0; $i < $num_rows; $i++) {
				$res->data_seek($i);
				$datarow = $res->fetch_array();
				$user = $datarow[$user_offest];
				$msg .= $user;
				if($i < $num_rows-1) {
					$msg .= $separator;
				}
			}
			set_result("success", $cmd, $msg);
		}
		
		$dbo->close();
	}

	//execute the command, now validated, given its argument; add commands as list of valid commands grows
	function process_command($user, $input) {
		$cmd = $input["cmd"];
		
		if($cmd == "move") {
			process_move($user, $input);
		}
		else if($cmd == "say" || $cmd == "yell") {
			process_msg($user, $input);
		}
		else if($cmd == "list") {
			process_list($user, $input);
		}
	}

	function printout($msg) {
		echo $msg . "\n";
	}

	function get_http_field($msg, $fld_title) {
		$lineSep = "\r\n";
		
		$fld_begin = strpos($msg, $fld_title) + strlen($fld_title) + 2;
		$fld = substr($msg, $fld_begin, strlen($msg));
		$fld_end = strpos($fld, $lineSep);
		$fld = substr($fld, 0, $fld_end);
		
		return $fld;
	}

	function get_mode($input) {
		$mode_bgn = strlen("mode=");
		$mode_len = strpos($input, ";") - $mode_bgn;
		$mode = substr($input, $mode_bgn, $mode_len);
		return $mode;	
	}

	function get_user_name($input) {
		$name_bgn = strpos($input, ";") + strlen("user=") + 1;
		$name_len = strlen($input) - $name_bgn;
		$user = substr($input, $name_bgn, $name_len);
		return $user;	
	}

	function get_handshake_msg($msg) {
		//get the secKey
		$secKey = get_http_field($msg, "Sec-WebSocket-Key");
		$host_port = get_http_field($msg, "Host");
		$sep_loc = strpos($host_port, ":");
		$host = substr($host_port, 0 , $sep_loc);
		$port = substr($host_port, $sep_loc+1, strlen($host_port));
		//printout("sec key:" . $secKey . "~~~");
		//printout("host:" . $host . "\n" . "port:" . $port . "\n");
		
		$secAccept = base64_encode(pack('H*', sha1($secKey . '258EAFA5-E914-47DA-95CA-C5AB0DC85B11')));
		$upgrade  = "HTTP/1.1 101 Web Socket Protocol Handshake\r\n" .
		"Upgrade: websocket\r\n" .
		"Connection: Upgrade\r\n" .
		"WebSocket-Origin: $host\r\n" .
		"WebSocket-Location: ws://$host:$port/proto/proto.php\r\n" .
		"Sec-WebSocket-Accept: $secAccept\r\n" .
		"Sec-WebSocket-Version: 13\r\n\r\n";
		return $upgrade;
	}

	function decode_message($msg) {
		$length = $msg[1] & 0b01111111;
		
		$mask_idx = 2;
		if($length == 126) {
			$mask_idx = 4;
		}
		else if($length == 127) {
			$mask_idx = 10;
		}
		
		$mask_len = 4;
		$msg_start = $mask_idx + $mask_len;
		$msg_len = strlen($msg) - $msg_start;
		$mask = substr($msg, $mask_idx, $mask_len);
		//reduce overhead in loop below
		$msg = substr($msg, $msg_start, $msg_len);

		$decoded_msg = "";
		for($i = 0; $i < strlen($msg); $i++) {
			$decoded_msg .= $msg[$i] ^ $mask[$i % 4];
		}
		
		echo "recieved from client: $decoded_msg\n";
		if($decoded_msg == "PING") {
			echo "client ping - server did nothing\n";	
		}
		
		$decoded_msg = json_decode($decoded_msg, true);
		return $decoded_msg;
	}

	function encode_message($msg) {
		$utf8_msg = utf8_encode($msg);
		$size = strlen($utf8_msg);
		
		$size_byte = chr(0) | $size;
		
		//text type websocket; one msg length max
		$header = pack("C*", "10000001", $size_byte);
		$header .= $utf8_msg;
		
		return $header;
	}

	while(1) {
		// create a copy, so $clients doesn't get modified by socket_select()
        $read = $clients;
		
		// get a list of all the clients that have data to be read from - return immediately
        if (socket_select($read, $write = NULL, $except = NULL, 0) < 1)
            continue;
		
		 // check if there is a client trying to connect
        if (in_array($socket, $read)) {
            // accept the client, and add him to the $clients array
            $clients[] = $new_socket = socket_accept($socket);
			
			//a newly accepted client will want to handshake/upgrade the connection
			$data = socket_read($new_socket, 1024, PHP_BINARY_READ);
			if ($data != false && !empty(trim($data))) {
				$msg = get_handshake_msg($data);
				socket_write($new_socket, $msg, strlen($msg));
			}
            //socket_getpeername($new_socket, $ip);
            //echo "New client connected: {$ip}\n";
           
            // remove the listening socket from the clients-with-data array
            $key = array_search($socket, $read);
            unset($read[$key]);
        }
		
		// loop through all the clients that have data to read from
        foreach ($read as $read_socket) {
            // messages between client and server are short; 1024 bytes should be plenty
            // socket_read while show errors when the client is disconnected, so silence the error messages
			//@
            $data = socket_read($read_socket, 1024, PHP_BINARY_READ);
            if ($data === false) {
                // client is disconnected, remove from $clients array
                $key = array_search($read_socket, $clients);
                unset($clients[$key]);
                echo "client disconnected.\n";
                // continue to the next client to read from, if any
                continue;
            }
			$data = trim($data);
			
            if (!empty($data)) {
				$input = decode_message($data);
				
				if($input) {//client sent some data
					$mode = $input["mode"];
					$user_name = $input["user"];

					if($mode == "new") {
						$user_name = $input["user"];
						echo "new user:$user_name\n";
						add_new_user($user_name);
					}
					else if ($mode == "quit") {
						remove_user($user_name);
						echo "user:$user_name closed connection\n";
					}
					else if($mode == "existing") {
						$input = trim($input["cmd"]);
						$input = validate_cmd($input);
						if($input) {
							if(validate_arg($input)) {
								process_command($user_name, $input);
							}
						}
					}
					$response = json_encode($GLOBALS["result"]);
					$response = encode_message($response);

					if($response) {
						$num_written = socket_write($read_socket, $response, strlen($response));
					}
				}
            }
			
		} // end of reading foreach
    }

    socket_close($socket);
?>
