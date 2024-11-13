let bleDevice;
let gattServer;
let Theservice;
let writeCharacteristic;
let reconnectTrys = 0;

let canvas;
let epdDriver;
let startTime;
let chunkSize = 38;

function resetVariables() {
	gattServer = null;
	Theservice = null;
	writeCharacteristic = null;
	document.getElementById("log").value = '';
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

async function setDriver() {
	epdDriver = document.getElementById("epddriver").value;
	let pins = document.getElementById("epdpins").value;
	await sendcmd("00" + pins);
}

async function clearscreen() {
	if(confirm('确认清除屏幕内容?')) {
		await sendcmd("01" + epdDriver);
		await sendcmd("02");
		await sendcmd("06");
	}
}

async function sendIMGArray(imgArray, type = 'bw'){
	const count = Math.round(imgArray.length / chunkSize);
	let chunkIdx = 0;

	for (let i = 0; i < imgArray.length; i += chunkSize) {
		let currentTime = (new Date().getTime() - startTime) / 1000.0;
		let chunk = imgArray.substring(i, i + chunkSize);
		setStatus('正在发送' + (type === 'bwr' ? "红色" : '黑白') + '块: '
			+ (chunkIdx+1) + "/" + (count+1) + ", 用时: " + currentTime + "s");
		addLog('Sending chunk: ' + chunk);
		await sendCommand(hexToBytes("04" + chunk))
		chunkIdx++;
	}
}

async function sendimg(cmdIMG) {
	startTime = new Date().getTime();
	let imgArray = cmdIMG.replace(/(?:\r\n|\r|\n|,|0x| )/g, '');
	const bwArrLen = (canvas.width/8) * canvas.height * 2;

	await sendcmd("01" + epdDriver);
	if (imgArray.length == bwArrLen * 2) {
		await sendcmd("0310");
		await sendIMGArray(imgArray.slice(0, bwArrLen - 1));
		await sendcmd("0313");
		await sendIMGArray(imgArray.slice(bwArrLen), 'bwr');
	} else {
		await sendcmd("0313");
		await sendIMGArray(imgArray);
	}
	await sendcmd("05");

	let sendTime = (new Date().getTime() - startTime) / 1000.0;
	addLog("Done! Time used: " + sendTime + "s");
	setStatus("发送完成！耗时: " + sendTime + "s");

	await sendcmd("06");
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
		const ctx = canvas.getContext("2d");
		ctx.fillStyle = 'white';
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		document.getElementById('cmdIMAGE').value = '';
	}
}

function convert_dithering() {
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
	epdDriver = document.getElementById("epddriver").value;
	canvas = document.getElementById('canvas');

	updateButtonStatus();
	bytes2canvas(hexToBytes(document.getElementById('cmdIMAGE').value), canvas);

	document.getElementById('dithering').value = 'none';
}