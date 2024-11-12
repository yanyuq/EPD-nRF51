let bleDevice;
let gattServer;
let Theservice;
let writeCharacteristic;
let reconnectTrys = 0;

let imgArray = "";
let imgArrayLen = 0;
let chunkSize = 38;
let uploadPart = 0;
let totalPart = 0;

function resetVariables() {
	gattServer = null;
	Theservice = null;
	writeCharacteristic = null;
	document.getElementById("log").value = '';
	imgArray = "";
	imgArrayLen = 0;
	uploadPart = 0;
}

function handleError(error) {
	console.log(error);
	resetVariables();
	if (bleDevice == null)
		return;
	if (reconnectTrys <= 5) {
		reconnectTrys++;
		connect();
	}
	else {
		addLog("Was not able to connect, aborting");
		reconnectTrys = 0;
	}
}

async function sendCommand(cmd) {
	if (writeCharacteristic) {
		await writeCharacteristic.writeValue(cmd);
	}
}

async function sendcmd(cmdTXT) {
	let cmd = hexToBytes(cmdTXT);
	addLog('Send CMD: ' + cmdTXT);
	await sendCommand(cmd);
}

function setDriver() {
	let driver = document.getElementById("epddriver").value;
	let pins = document.getElementById("epdpins").value;
	sendcmd("00" + pins).then(() => {
		sendcmd("01" + driver);
	});
}

function clearscreen() {
	if(confirm('确认清除屏幕内容?')) {
		sendcmd("01").then(() => {
			sendcmd("02").then(() => {
				sendcmd("06");
			})
		}).catch(handleError);
	}
}

function sendimg(cmdIMG) {
	startTime = new Date().getTime();
	imgArray = cmdIMG.replace(/(?:\r\n|\r|\n|,|0x| )/g, '');
	imgArrayLen = imgArray.length;
	uploadPart = 0;
	totalPart = Math.round(imgArrayLen / chunkSize);
	console.log('Sending image ' + imgArrayLen);
	sendcmd("01").then(() => {
		sendCommand(hexToBytes("0313")).then(() => {
			sendIMGpart();
		});
	}).catch(handleError);
}

function sendIMGpart() {
	if (imgArray.length > 0) {
		let currentPart = imgArray.substring(0, chunkSize);
		let currentTime = (new Date().getTime() - startTime) / 1000.0;
		imgArray = imgArray.substring(chunkSize);
		setStatus('正在发送块: ' + (uploadPart++) + "/" + totalPart + ", 用时: " + currentTime + "s");
		addLog('Sending Part: ' + currentPart);
		sendCommand(hexToBytes("04" + currentPart)).then(() => {
			sendIMGpart();
		})
	} else {
		sendCommand(hexToBytes("05")).then(() => {
			let sendTime = (new Date().getTime() - startTime) / 1000.0;
			addLog("Done! Time used: " + sendTime + "s");
			setStatus("发送完成！耗时: " + sendTime + "s");
			sendcmd("06");
		})
	}
}

function updateButtonStatus() {
	let connected = gattServer != null && gattServer.connected;
	let status = connected ? null : 'disabled';
	document.getElementById("sendcmdbutton").disabled = status;
	document.getElementById("clearscreenbutton").disabled = status;
	document.getElementById("sendimgbutton").disabled = status;
	document.getElementById("setDriverbutton").disabled = status;
}

function disconnect() {
	resetVariables();
	addLog('Disconnected.');
	document.getElementById("connectbutton").innerHTML = '连接';
	updateButtonStatus();
}

function preConnect() {
	if (gattServer != null && gattServer.connected) {
		if (bleDevice != null && bleDevice.gatt.connected)
			bleDevice.gatt.disconnect();
	}
	else {
		connectTrys = 0;
		navigator.bluetooth.requestDevice({ optionalServices: ['62750001-d828-918d-fb46-b6c11c675aec'], acceptAllDevices: true }).then(device => {
			device.addEventListener('gattserverdisconnected', disconnect);
			bleDevice = device;
			connect();
		}).catch(handleError);
	}
}

function reConnect() {
	connectTrys = 0;
	if (bleDevice != null && bleDevice.gatt.connected)
		bleDevice.gatt.disconnect();
	resetVariables();
	addLog("Reconnect");
	setTimeout(function () { connect(); }, 300);
}

function connect() {
	if (writeCharacteristic == null) {
		addLog("Connecting to: " + bleDevice.name);
		bleDevice.gatt.connect().then(server => {
			addLog('> Found GATT Server');
			gattServer = server;
			return gattServer.getPrimaryService('62750001-d828-918d-fb46-b6c11c675aec');
		}).then(service => {
			addLog('> Found Service');
			Theservice = service;
			return Theservice.getCharacteristic('62750002-d828-918d-fb46-b6c11c675aec');
		}).then(characteristic => {
			addLog('> Found Characteristic');
			document.getElementById("connectbutton").innerHTML = '断开';
			updateButtonStatus();
			writeCharacteristic = characteristic;
			return;
		}).catch(handleError);
	}
}

function setStatus(statusText) {
	document.getElementById("status").innerHTML = statusText;
}

function addLog(logTXT) {
	var today = new Date();
	var time = ("0" + today.getHours()).slice(-2) + ":" + ("0" + today.getMinutes()).slice(-2) + ":" + ("0" + today.getSeconds()).slice(-2) + " : ";
	document.getElementById("log").innerHTML += time + logTXT + '<br>';
	console.log(time + logTXT);
	while ((document.getElementById("log").innerHTML.match(/<br>/g) || []).length > 10) {
		var logs_br_position = document.getElementById("log").innerHTML.search("<br>");
		document.getElementById("log").innerHTML = document.getElementById("log").innerHTML.substring(logs_br_position + 4);
	}
}

function hexToBytes(hex) {
	for (var bytes = [], c = 0; c < hex.length; c += 2)
		bytes.push(parseInt(hex.substr(c, 2), 16));
	return new Uint8Array(bytes);
}

function bytesToHex(data) {
	return new Uint8Array(data).reduce(
		function (memo, i) {
			return memo + ("0" + i.toString(16)).slice(-2);
		}, "");
}

function intToHex(intIn) {
	var stringOut = "";
	stringOut = ("0000" + intIn.toString(16)).substr(-4)
	return stringOut.substring(2, 4) + stringOut.substring(0, 2);
}

function updateImageData(canvas) {
	document.getElementById('cmdIMAGE').value = bytesToHex(canvas2bytes(canvas, 'bw'));
	if (document.getElementById("epddriver").value == '03' &&
		document.getElementById('dithering').value.startsWith('bwr')) {
		document.getElementById('cmdIMAGE').value += bytesToHex(canvas2bytes(canvas, 'bwr'));
	}
}

async function update_image () {
	const image_file = document.getElementById('image_file');
	if (image_file.files.length > 0) {
		const file = image_file.files[0];

		const canvas = document.getElementById("canvas");
		const ctx = canvas.getContext("2d");

		const image = new Image();
		image.src = URL.createObjectURL(file);
		image.onload = function(event) {
			URL.revokeObjectURL(this.src);
			ctx.drawImage(image, 0, 0, image.width, image.height, 0, 0, canvas.width, canvas.height);
			convert_dithering()
		}
	}
}

function clear_canvas() {
	if(confirm('确认清除画布内容?')) {
		const canvas = document.getElementById('canvas');
		const ctx = canvas.getContext("2d");
		ctx.fillStyle = 'white';
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		document.getElementById('cmdIMAGE').value = '';
	}
}

function convert_dithering() {
	const canvas = document.getElementById('canvas');
	const ctx = canvas.getContext("2d");
	const mode = document.getElementById('dithering').value;
	if (mode.startsWith('bwr')) {
		ditheringCanvasByPalette(canvas, bwrPalette, mode);
	} else {
		dithering(ctx, canvas.width, canvas.height, parseInt(document.getElementById('threshold').value), mode);
	}
	updateImageData(canvas);
}

document.body.onload = () => {
	updateButtonStatus();

	const canvas = document.getElementById('canvas');
	bytes2canvas(hexToBytes(document.getElementById('cmdIMAGE').value), canvas);

	document.getElementById('dithering').value = 'none';
}