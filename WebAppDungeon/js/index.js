var userName;
var commandList = ["move", "say", "yell"];
var transferMethod = "GET";
var serverURL = "localhost:6789";
var xhr = false;
var connection = false; //new WebSocket("ws://" + serverURL);

createSocket();

function checkName() {
	if(!connection) {
		outputUsernameError("Waiting for server connection. Please try again in a moment.");
	}
	var name = document.forms["username_form"].elements["username_input"].value;
	if(name == "" || name == null) {
		checkName();
		return;
	}
	userName = name;
	
	var msgArr = {};
	msgArr["mode"] = "new";
	msgArr["user"] = userName;
	
	var jsonMsg = JSON.stringify(msgArr);
	updateServer(jsonMsg);
}

function showGame() {
	var interactionBox = document.getElementById("interaction_box");
	
	//get rid of login form
	var loginBox = document.getElementById("username_div");
	interactionBox.removeChild(loginBox);
	
	//create command output window
	var msgBox = document.createElement("div");
	msgBox.setAttribute("id", "msg_box");
	interactionBox.appendChild(msgBox);
	
	//create command input window and form
	var inputBox = document.createElement("div");
	inputBox.setAttribute("id", "input_box");
	var inputForm = document.createElement("form");
	inputForm.setAttribute("id", "command_form");
	inputForm.setAttribute("onsubmit", "sendCommand(); return false;");
	var inputWindowMsg = document.createElement("p");
	inputWindowMsg.setAttribute("id", "command_msg");
	inputWindowMsg.innerHTML = "Enter Command: ";
	var input = document.createElement("input");
	input.setAttribute("id", "command_input");
	input.setAttribute("type", "text");
	input.setAttribute("name", "command");
	
	//build game window
	inputForm.appendChild(inputWindowMsg);
	inputForm.appendChild(input);
	inputBox.appendChild(inputForm);
	interactionBox.appendChild(msgBox);
	interactionBox.appendChild(inputBox);
}

function outputMsg(speaker, msg) {
	var msgBox = document.getElementById("msg_box");
	var outputP = document.createElement("p"); 
	var brk = document.createElement("br");
	
	outputP.setAttribute("class", "txt_output");
	outputP.innerHTML = speaker + "> " + msg;
	msgBox.appendChild(outputP);
	msgBox.appendChild(brk);
	
	msgBox.scrollTop = msgBox.scrollHeight;
}

function removeServerOutput() {
	var commandMsg = document.getElementById("error_msg");
	if(commandMsg != null) {
		document.getElementById("command_msg").removeChild(commandMsg);
	}
}

function outputServerResponse(msg) {
	removeServerOutput();
	var commandMsg = document.getElementById("command_msg");
	var errorP = document.createElement("p");
	errorP.setAttribute("id", "error_msg");
	errorP.innerHTML = msg;
	commandMsg.appendChild(errorP);
}

//use to send commands issued by the user
function updateServer(message) {
	if(connection) {
		console.log("sending:" + message);
		connection.send(message);
	}
}

window.onbeforeunload = function() {
	var msgArr = {};
	msgArr["mode"] = "quit";
	msgArr["user"] = userName;

	var jsonMsg = JSON.stringify(msgArr);
	updateServer(jsonMsg);
};

function sendCommand() {
	var input = document.forms["command_form"].elements["command_input"].value;
	
	var msgArr = {};
	msgArr["mode"] = "existing";
	msgArr["user"] = userName;
	msgArr["cmd"] = input;
	
	var jsonMsg = JSON.stringify(msgArr);
	updateServer(jsonMsg);
}

function processServerPush(e) {
	console.log("data:" + e.data);
}

function createSocket() {
	connection = new WebSocket("ws://" + serverURL);
	
	//connection.onmessage = processServerPush;
	connection.onopen = function(evt) {
		console.log("socket opened\n");
	};
	
	connection.onerror = function(evt) {
		console.log("socket error: " + evt.data + "\n");
	};
	
	connection.onclose = function(evt) {
		console.log("socket closed\n");
	};
	
	connection.onmessage = function(evt) { 
		var received_msg = evt.data;
		console.log("Message received:" + received_msg);
		var res = JSON.parse(evt.data);
		if(res.res != "error") {
			removeServerOutput();
		}
		
		//server sends back "Weclome" if client successfully 'logged in'
		if(res.msg == "Welcome") {
			showGame();
		}
		outputServerResponse(res.msg);
	};
	
	connection.readyState.onchange = function() {
		console.log("socket state:" + connection.readyState + "\n");
	};
}
