let bleDevice;
let gattServer;
let epdService;
let epdCharacteristic;
let reconnectTrys = 0;

let canvas;
let startTime;
let chunkSize = 38;

function resetVariables() {
	gattServer = null;
	epdService = null;
	epdCharacteristic = null;
	document.getElementById("log").value = '';
}

async function handleError(error) {
	console.log(error);
	resetVariables();
	if (bleDevice == null)
		return;
	if (reconnectTrys <= 5) {
		reconnectTrys++;
		await connect();
	}
	else {
		addLog("连接失败！");
		reconnectTrys = 0;
	}
}

async function sendCommand(cmd) {
	if (epdCharacteristic) {
		await epdCharacteristic.writeValue(cmd);
	} else {
		addLog("服务不可用，请检查蓝牙连接");
	}
}

async function sendcmd(cmdTXT) {
	let cmd = hexToBytes(cmdTXT);
	addLog(`发送命令: ${cmdTXT}`);
	await sendCommand(cmd);
}

async function setDriver() {
	let epdDriver = document.getElementById("epddriver").value;
	let pins = document.getElementById("epdpins").value;
	await sendcmd("00" + pins);
	await sendcmd("01" + epdDriver);
	await sendcmd("06");
}

async function clearscreen() {
	if(confirm('确认清除屏幕内容?')) {
		await sendcmd("01");
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
		setStatus(`发送${type === 'bwr' ? "红色" : '黑白'}块: ${chunkIdx+1}/${count+1}, 用时: ${currentTime}s`);
		addLog(`发送块: ${chunk}`);
		await sendCommand(hexToBytes(`04${chunk}`))
		chunkIdx++;
	}
}

async function sendimg(cmdIMG) {
	startTime = new Date().getTime();
	let epdDriver = document.getElementById("epddriver").value;
	let imgArray = cmdIMG.replace(/(?:\r\n|\r|\n|,|0x| )/g, '');
	const bwArrLen = (canvas.width/8) * canvas.height * 2;

	await sendcmd("01");
	if (imgArray.length == bwArrLen * 2) {
		await sendcmd("0310");
		await sendIMGArray(imgArray.slice(0, bwArrLen - 1));
		await sendcmd("0313");
		await sendIMGArray(imgArray.slice(bwArrLen), 'bwr');
	} else {
		await sendcmd(epdDriver === "03" ? "0310" : "0313");
		await sendIMGArray(imgArray);
	}
	await sendcmd("05");

	let sendTime = (new Date().getTime() - startTime) / 1000.0;
	addLog(`发送完成！耗时: ${sendTime}s`);
	setStatus(`发送完成！耗时: ${sendTime}s`);

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
	addLog('已断开连接.');
	document.getElementById("connectbutton").innerHTML = '连接';
	updateButtonStatus();
}

async function preConnect() {
	if (gattServer != null && gattServer.connected) {
		if (bleDevice != null && bleDevice.gatt.connected)
			bleDevice.gatt.disconnect();
	}
	else {
		connectTrys = 0;
		bleDevice = await navigator.bluetooth.requestDevice({
			optionalServices: ['62750001-d828-918d-fb46-b6c11c675aec'],
			acceptAllDevices: true
		});
		await bleDevice.addEventListener('gattserverdisconnected', disconnect);
		try {
			await connect();
		} catch (e) {
			await handleError(e);
		}
	}
}

async function reConnect() {
	connectTrys = 0;
	if (bleDevice != null && bleDevice.gatt.connected)
		bleDevice.gatt.disconnect();
	resetVariables();
	addLog("正在重连");
	setTimeout(async function () { await connect(); }, 300);
}

async function connect() {
	if (epdCharacteristic == null) {
		addLog("正在连接: " + bleDevice.name);

		gattServer = await bleDevice.gatt.connect();
		addLog('> 找到 GATT Server');

		epdService = await gattServer.getPrimaryService('62750001-d828-918d-fb46-b6c11c675aec');
		addLog('> 找到 EPD Service');

		epdCharacteristic = await epdService.getCharacteristic('62750002-d828-918d-fb46-b6c11c675aec');
		addLog('> 找到 Characteristic');

		await epdCharacteristic.startNotifications();
		epdCharacteristic.addEventListener('characteristicvaluechanged', (event) => {
			addLog(`> 收到配置：${bytesToHex(event.target.value.buffer.slice(0,8))}`);
			document.getElementById("epdpins").value = bytesToHex(event.target.value.buffer.slice(0, 7));
			document.getElementById("epddriver").value = bytesToHex(event.target.value.buffer.slice(7, 8));
		});

		document.getElementById("connectbutton").innerHTML = '断开';
		updateButtonStatus();
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
	if (document.getElementById('dithering').value.startsWith('bwr')) {
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
	canvas = document.getElementById('canvas');

	updateButtonStatus();
	bytes2canvas(hexToBytes(document.getElementById('cmdIMAGE').value), canvas);

	document.getElementById('dithering').value = 'none';
}