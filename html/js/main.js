let bleDevice, gattServer;
let epdService, epdCharacteristic;
let startTime, msgIndex, appVersion;
let canvas, ctx, textDecoder;
// 文字相关变量
let textAddedToCanvas = false;
let textContent = '';
let textSize = 30;
let textColor = '#000000';
let textX = 0;
let textY = 0;
let textDragging = false;
let dragStartX = 0;
let dragStartY = 0;
// 记录原始画布内容的变量，用于拖拽时恢复
let originalImageData = null;

const EpdCmd = {
  SET_PINS:  0x00,
  INIT:      0x01,
  CLEAR:     0x02,
  SEND_CMD:  0x03,
  SEND_DATA: 0x04,
  REFRESH:   0x05,
  SLEEP:     0x06,

  SET_TIME:  0x20,

  WRITE_IMG: 0x30, // v1.6

  SET_CONFIG: 0x90,
  SYS_RESET:  0x91,
  SYS_SLEEP:  0x92,
  CFG_ERASE:  0x99,
};

function resetVariables() {
  gattServer = null;
  epdService = null;
  epdCharacteristic = null;
  msgIndex = 0;
  document.getElementById("log").value = '';
}

async function write(cmd, data, withResponse=true) {
  if (!epdCharacteristic) {
    addLog("服务不可用，请检查蓝牙连接");
    return false;
  }
  let payload = [cmd];
  if (data) {
    if (typeof data == 'string') data = hex2bytes(data);
    if (data instanceof Uint8Array) data = Array.from(data);
    payload.push(...data)
  }
  addLog(bytes2hex(payload), '⇑');
  try {
    if (withResponse)
      await epdCharacteristic.writeValueWithResponse(Uint8Array.from(payload));
    else
      await epdCharacteristic.writeValueWithoutResponse(Uint8Array.from(payload));
  } catch (e) {
    console.error(e);
    if (e.message) addLog("write: " + e.message);
    return false;
  }
  return true;
}

async function epdWrite(cmd, data) {
  const chunkSize = document.getElementById('mtusize').value - 1;
  const interleavedCount = document.getElementById('interleavedcount').value;
  const count = Math.round(data.length / chunkSize);
  let chunkIdx = 0;
  let noReplyCount = interleavedCount;

  if (typeof data == 'string') data = hex2bytes(data);

  await write(EpdCmd.SEND_CMD, [cmd]);
  for (let i = 0; i < data.length; i += chunkSize) {
    let currentTime = (new Date().getTime() - startTime) / 1000.0;
    setStatus(`命令：0x${cmd.toString(16)}, 数据块: ${chunkIdx+1}/${count+1}, 总用时: ${currentTime}s`);
    if (noReplyCount > 0) {
      await write(EpdCmd.SEND_DATA, data.slice(i, i + chunkSize), false);
      noReplyCount--;
    } else {
      await write(EpdCmd.SEND_DATA, data.slice(i, i + chunkSize), true);
      noReplyCount = interleavedCount;
    }
    chunkIdx++;
  }
}

async function epdWriteImage(step = 'bw') {
  const data = canvas2bytes(canvas, step);
  const chunkSize = document.getElementById('mtusize').value - 2;
  const interleavedCount = document.getElementById('interleavedcount').value;
  const count = Math.round(data.length / chunkSize);
  let chunkIdx = 0;
  let noReplyCount = interleavedCount;

  for (let i = 0; i < data.length; i += chunkSize) {
    let currentTime = (new Date().getTime() - startTime) / 1000.0;
    setStatus(`${step == 'bw' ? '黑白' : '红色'}块: ${chunkIdx+1}/${count+1}, 总用时: ${currentTime}s`);
    const payload = [
      (step == 'bw' ? 0x0F : 0x00) | ( i == 0 ? 0x00 : 0xF0),
      ...data.slice(i, i + chunkSize),
    ];
    if (noReplyCount > 0) {
      await write(EpdCmd.WRITE_IMG, payload, false);
      noReplyCount--;
    } else {
      await write(EpdCmd.WRITE_IMG, payload, true);
      noReplyCount = interleavedCount;
    }
    chunkIdx++;
  }
}

async function setDriver() {
  await write(EpdCmd.SET_PINS, document.getElementById("epdpins").value);
  await write(EpdCmd.INIT, document.getElementById("epddriver").value);
}

async function syncTime(mode) {
  const timestamp = new Date().getTime() / 1000;
  const data = new Uint8Array([
    (timestamp >> 24) & 0xFF,
    (timestamp >> 16) & 0xFF,
    (timestamp >> 8) & 0xFF,
    timestamp & 0xFF,
    -(new Date().getTimezoneOffset() / 60),
    mode
  ]);
  if(await write(EpdCmd.SET_TIME, data)) {
    addLog("时间已同步！");
  }
}

async function clearScreen() {
  if(confirm('确认清除屏幕内容?')) {
    await write(EpdCmd.CLEAR);
  }
}

async function sendcmd() {
  const cmdTXT = document.getElementById('cmdTXT').value;
  if (cmdTXT == '') return;
  const bytes = hex2bytes(cmdTXT);
  await write(bytes[0], bytes.length > 1 ? bytes.slice(1) : null);
}

async function sendimg() {
  const status = document.getElementById("status");
  const driver = document.getElementById("epddriver").value;
  const mode = document.getElementById('dithering').value;

  if (mode === '') {
    alert('请选择一种取模算法！');
    return;
  }

  startTime = new Date().getTime();
  status.parentElement.style.display = "block";

  if (appVersion < 0x16) {
    if (mode.startsWith('bwr')) {
      await epdWrite(driver === "02" ? 0x24 : 0x10, canvas2bytes(canvas, 'bw'));
      await epdWrite(driver === "02" ? 0x26 : 0x13, canvas2bytes(canvas, 'red', driver === '02'));
    } else {
      await epdWrite(driver === "04" ? 0x24 : 0x13, canvas2bytes(canvas, 'bw'));
    }
  } else {
    await epdWriteImage('bw');
    if (mode.startsWith('bwr')) await epdWriteImage('red');
  }

  await write(EpdCmd.REFRESH);

  const sendTime = (new Date().getTime() - startTime) / 1000.0;
  addLog(`发送完成！耗时: ${sendTime}s`);
  setStatus(`发送完成！耗时: ${sendTime}s`);
  setTimeout(() => {
    status.parentElement.style.display = "none";
  }, 5000);
}

function updateButtonStatus() {
  const connected = gattServer != null && gattServer.connected;
  const status = connected ? null : 'disabled';
  document.getElementById("reconnectbutton").disabled = (gattServer == null || gattServer.connected) ? 'disabled' : null;
  document.getElementById("sendcmdbutton").disabled = status;
  document.getElementById("calendarmodebutton").disabled = status;
  document.getElementById("clockmodebutton").disabled = status;
  document.getElementById("clearscreenbutton").disabled = status;
  document.getElementById("sendimgbutton").disabled = status;
  document.getElementById("setDriverbutton").disabled = status;
}

function disconnect() {
  updateButtonStatus();
  resetVariables();
  addLog('已断开连接.');
  document.getElementById("connectbutton").innerHTML = '连接';
}

async function preConnect() {
  if (gattServer != null && gattServer.connected) {
    if (bleDevice != null && bleDevice.gatt.connected) {
      bleDevice.gatt.disconnect();
    }
  }
  else {
    resetVariables();
    try {
      bleDevice = await navigator.bluetooth.requestDevice({
        optionalServices: ['62750001-d828-918d-fb46-b6c11c675aec'],
        acceptAllDevices: true
      });
    } catch (e) {
      console.error(e);
      if (e.message) addLog("requestDevice: " + e.message);
      addLog("请检查蓝牙是否已开启，且使用的浏览器支持蓝牙！建议使用以下浏览器：");
      addLog("• 电脑: Chrome/Edge");
      addLog("• Android: Chrome/Edge");
      addLog("• iOS: Bluefy 浏览器");
      return;
    }

    await bleDevice.addEventListener('gattserverdisconnected', disconnect);
    setTimeout(async function () { await connect(); }, 300);
  }
}

async function reConnect() {
  if (bleDevice != null && bleDevice.gatt.connected)
    bleDevice.gatt.disconnect();
  resetVariables();
  addLog("正在重连");
  setTimeout(async function () { await connect(); }, 300);
}

function handleNotify(value, idx) {
  const data = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  if (idx == 0) {
    addLog(`收到配置：${bytes2hex(data)}`);
    const epdpins = document.getElementById("epdpins");
    const epddriver = document.getElementById("epddriver");
    epdpins.value = bytes2hex(data.slice(0, 7));
    if (data.length > 10) epdpins.value += bytes2hex(data.slice(10, 11));
    epddriver.value = bytes2hex(data.slice(7, 8));
    filterDitheringOptions();
  } else {
    if (textDecoder == null) textDecoder = new TextDecoder();
    addLog(textDecoder.decode(data), '⇓');
  }
}

async function connect() {
  if (bleDevice == null || epdCharacteristic != null) return;

  try {
    addLog("正在连接: " + bleDevice.name);
    gattServer = await bleDevice.gatt.connect();
    addLog('  找到 GATT Server');
    epdService = await gattServer.getPrimaryService('62750001-d828-918d-fb46-b6c11c675aec');
    addLog('  找到 EPD Service');
    epdCharacteristic = await epdService.getCharacteristic('62750002-d828-918d-fb46-b6c11c675aec');
    addLog('  找到 Characteristic');
  } catch (e) {
    console.error(e);
    if (e.message) addLog("connect: " + e.message);
    disconnect();
    return;
  }

  try {
    const versionCharacteristic = await epdService.getCharacteristic('62750003-d828-918d-fb46-b6c11c675aec');
    const versionData = await versionCharacteristic.readValue();
    appVersion = versionData.getUint8(0);
    addLog(`固件版本: 0x${appVersion.toString(16)}`);
  } catch (e) {
    console.error(e);
    appVersion = 0x15;
  }

  try {
    await epdCharacteristic.startNotifications();
    epdCharacteristic.addEventListener('characteristicvaluechanged', (event) => {
      handleNotify(event.target.value, msgIndex++);
    });
  } catch (e) {
    console.error(e);
    if (e.message) addLog("startNotifications: " + e.message);
  }

  await write(EpdCmd.INIT);

  document.getElementById("connectbutton").innerHTML = '断开';
  updateButtonStatus();
}

function setStatus(statusText) {
  document.getElementById("status").innerHTML = statusText;
}

function addLog(logTXT, action = '') {
  const log = document.getElementById("log");
  const now = new Date();
  const time = String(now.getHours()).padStart(2, '0') + ":" +
         String(now.getMinutes()).padStart(2, '0') + ":" +
         String(now.getSeconds()).padStart(2, '0') + " ";

  const logEntry = document.createElement('div');
  const timeSpan = document.createElement('span');
  timeSpan.className = 'time';
  timeSpan.textContent = time;
  logEntry.appendChild(timeSpan);
  
  if (action !== '') {
    const actionSpan = document.createElement('span');
    actionSpan.className = 'action';
    actionSpan.innerHTML = action;
    logEntry.appendChild(actionSpan);
  }
  logEntry.appendChild(document.createTextNode(logTXT));

  log.appendChild(logEntry);
  log.scrollTop = log.scrollHeight;
  
  while (log.childNodes.length > 20) {
    log.removeChild(log.firstChild);
  }
}

function clearLog() {
  document.getElementById("log").innerHTML = '';
}

function hex2bytes(hex) {
  for (var bytes = [], c = 0; c < hex.length; c += 2)
    bytes.push(parseInt(hex.substr(c, 2), 16));
  return new Uint8Array(bytes);
}

function bytes2hex(data) {
  return new Uint8Array(data).reduce(
    function (memo, i) {
      return memo + ("0" + i.toString(16)).slice(-2);
    }, "");
}

function intToHex(intIn) {
  let stringOut = ("0000" + intIn.toString(16)).substr(-4)
  return stringOut.substring(2, 4) + stringOut.substring(0, 2);
}

async function update_image() {
  let image = new Image();;
  const image_file = document.getElementById('image_file');
  if (image_file.files.length > 0) {
    const file = image_file.files[0];
    image.src = URL.createObjectURL(file);
  } else {
    image.src = document.getElementById('demo-img').src;
  }

  image.onload = function(event) {
    URL.revokeObjectURL(this.src);
    ctx.drawImage(image, 0, 0, image.width, image.height, 0, 0, canvas.width, canvas.height);
    convert_dithering()
  }
}

function clear_canvas() {
  if(confirm('确认清除画布内容?')) {
    if (ctx) {
      ctx.fillStyle = "#FFFFFF";
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      // 重置文字状态
      textAddedToCanvas = false;
      // 保存新的画布状态
      saveCanvasState();
    }
  }
}

function convert_dithering() {
  const mode = document.getElementById('dithering').value;
  if (mode === '') return;

  if (mode.startsWith('bwr')) {
    ditheringCanvasByPalette(canvas, bwrPalette, mode);
  } else {
    dithering(ctx, canvas.width, canvas.height, parseInt(document.getElementById('threshold').value), mode);
  }
}

function filterDitheringOptions() {
  const driver = document.getElementById('epddriver').value;
  const dithering = document.getElementById('dithering');
  let currentOptionStillValid = false;

  for (let optgroup of dithering.getElementsByTagName('optgroup')) {
    const drivers = optgroup.getAttribute('data-driver').split('|');
    const show = drivers.includes(driver);
    for (option of optgroup.getElementsByTagName('option')) {
      if (show) {
        option.removeAttribute('disabled');
        if (option.value == dithering.value) currentOptionStillValid = true;
      } else {
        option.setAttribute('disabled', 'disabled');
      }
    }
  }
  if (!currentOptionStillValid) dithering.value = '';
}

function checkDebugMode() {
  const link = document.getElementById('debug-toggle');
  const urlParams = new URLSearchParams(window.location.search);
  const debugMode = urlParams.get('debug');
  
  if (debugMode === 'true') {
      document.body.classList.add('debug-mode');
      link.innerHTML = '正常模式';
      link.setAttribute('href', window.location.pathname);
      addLog("注意：开发模式功能已开启！不懂请不要随意修改，否则后果自负！");
  } else {
      document.body.classList.remove('debug-mode');
      link.innerHTML = '开发模式';
      link.setAttribute('href', window.location.pathname + '?debug=true');
  }
}

document.body.onload = () => {
  textDecoder = null;
  canvas = document.getElementById('canvas');
  ctx = canvas.getContext("2d");

  updateButtonStatus();
  update_image();
  filterDitheringOptions();

  checkDebugMode();
}

// 显示文本输入面板
function showTextInput() {
  document.getElementById('textInputPanel').style.display = 'flex';
  document.getElementById('textContent').value = textContent;
  document.getElementById('textSize').value = textSize;
  document.getElementById('textColor').value = textColor;
}

// 隐藏文本输入面板
function hideTextInput() {
  document.getElementById('textInputPanel').style.display = 'none';
}

// 添加文本到画布
function addTextToCanvas() {
  textContent = document.getElementById('textContent').value.trim();
  if (!textContent) {
    alert('请输入文本内容');
    return;
  }
  
  textSize = parseInt(document.getElementById('textSize').value);
  textColor = document.getElementById('textColor').value;
  
  // 默认将文字放在画布中心
  textX = canvas.width / 2;
  textY = canvas.height / 2;
  
  // 保存当前画布状态
  saveCanvasState();
  
  // 渲染文字
  drawTextOnCanvas();
  
  // 隐藏面板
  hideTextInput();
  
  // 添加拖动事件
  addDragEvents();
  
  textAddedToCanvas = true;
  
  addLog('已添加文字: "' + textContent + '"');
}

// 保存画布当前状态
function saveCanvasState() {
  try {
    originalImageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
  } catch (e) {
    console.error('保存画布状态失败', e);
    addLog('保存画布状态失败: ' + e.message);
  }
}

// 绘制文字到画布
function drawTextOnCanvas() {
  // 确保有原始图像数据，如果没有就直接在当前画布上绘制
  if (originalImageData) {
    // 恢复原始画布内容
    ctx.putImageData(originalImageData, 0, 0);
  }
  
  // 设置文字样式
  ctx.font = textSize + 'px Arial';
  ctx.fillStyle = textColor;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  
  // 绘制文字
  ctx.fillText(textContent, textX, textY);
}

// 添加拖动事件监听器
function addDragEvents() {
  canvas.classList.add('text-dragging');
  
  // 鼠标按下事件
  canvas.addEventListener('mousedown', handleMouseDown);
  
  // 鼠标移动事件
  canvas.addEventListener('mousemove', handleMouseMove);
  
  // 鼠标释放事件
  canvas.addEventListener('mouseup', handleMouseUp);
  
  // 触摸事件（移动设备）
  canvas.addEventListener('touchstart', handleTouchStart);
  canvas.addEventListener('touchmove', handleTouchMove);
  canvas.addEventListener('touchend', handleTouchEnd);
}

// 处理鼠标按下事件
function handleMouseDown(e) {
  if (!textAddedToCanvas) return;
  
  const rect = canvas.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;
  
  const textWidth = ctx.measureText(textContent).width;
  const textHeight = textSize;
  
  // 检查点击位置是否在文字范围内
  if (Math.abs(x - textX) < textWidth / 2 + 10 && 
      Math.abs(y - textY) < textHeight / 2 + 10) {
    textDragging = true;
    dragStartX = x;
    dragStartY = y;
  }
}

// 处理鼠标移动事件
function handleMouseMove(e) {
  if (!textDragging) return;
  
  const rect = canvas.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;
  
  const deltaX = x - dragStartX;
  const deltaY = y - dragStartY;
  
  textX += deltaX;
  textY += deltaY;
  
  dragStartX = x;
  dragStartY = y;
  
  // 重绘画布
  drawTextOnCanvas();
}

// 处理鼠标释放事件
function handleMouseUp() {
  textDragging = false;
}

// 处理触摸开始事件（移动设备）
function handleTouchStart(e) {
  if (!textAddedToCanvas) return;
  
  e.preventDefault();
  const touch = e.touches[0];
  const rect = canvas.getBoundingClientRect();
  const x = touch.clientX - rect.left;
  const y = touch.clientY - rect.top;
  
  const textWidth = ctx.measureText(textContent).width;
  const textHeight = textSize;
  
  // 检查触摸位置是否在文字范围内
  if (Math.abs(x - textX) < textWidth / 2 + 20 && 
      Math.abs(y - textY) < textHeight / 2 + 20) {
    textDragging = true;
    dragStartX = x;
    dragStartY = y;
  }
}

// 处理触摸移动事件（移动设备）
function handleTouchMove(e) {
  if (!textDragging) return;
  
  e.preventDefault();
  const touch = e.touches[0];
  const rect = canvas.getBoundingClientRect();
  const x = touch.clientX - rect.left;
  const y = touch.clientY - rect.top;
  
  const deltaX = x - dragStartX;
  const deltaY = y - dragStartY;
  
  textX += deltaX;
  textY += deltaY;
  
  dragStartX = x;
  dragStartY = y;
  
  // 重绘画布
  drawTextOnCanvas();
}

// 处理触摸结束事件（移动设备）
function handleTouchEnd() {
  textDragging = false;
}