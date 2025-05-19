// 定义蓝牙命令
const EpdCmd = {
  SET_PINS:   0x00,
  INIT:       0x01,
  CLEAR:      0x02,
  SEND_CMD:   0x03,
  SEND_DATA:  0x04,
  REFRESH:    0x05,
  SLEEP:      0x06,

  SET_TIME:   0x20,

  SET_CONFIG: 0x90,
  SYS_RESET:  0x91,
  SYS_SLEEP:  0x92,
  CFG_ERASE:  0x99,
};

// 为低版本微信添加ImageData polyfill
if (typeof ImageData === 'undefined') {
  // 简单的ImageData polyfill
  globalThis.ImageData = function ImageData(data, width, height) {
    this.data = data;
    this.width = width;
    this.height = height;
    return this;
  }
}

// 导入工具类
const util = require('../../utils/util.js');
const imageProcessing = require('../../utils/image-processing.js');

// 电子墨水屏服务和特征UUID
const EPD_SERVICE_UUID = '62750001-d828-918d-fb46-b6c11c675aec';
const EPD_CHARACTERISTIC_UUID = '62750002-d828-918d-fb46-b6c11c675aec';

Page({
  data: {
    connected: false,
    deviceName: '',
    logs: [],
    image: '',
    statusText: '',
    showStatus: false,
    epdDriver: '01', // 默认为UC8176（黑白屏）
    epdDriverIndex: 0, // 驱动选择器索引
    epdPins: '',
    dithering: 'none',
    threshold: 125,
    mtuSize: 20, // 默认MTU大小为20
    interleavedCount: 20,
    deviceConnected: false,
    scanning: false,
    scanText: '扫描',
    deviceList: [],
    currentServiceId: null,
    currentCharacteristicId: null,
    bwDitheringOptions: [],
    bwrDitheringOptions: [],
    ditheringIndex: 0,
    logScrollTop: 9999,
    driverOptions: [
      { value: '01', name: 'UC8176（黑白屏）' },
      { value: '03', name: 'UC8176（三色屏）' },
      { value: '05', name: 'UC8276（三色屏）' },
      { value: '04', name: 'SSD1619（黑白屏）' },
      { value: '02', name: 'SSD1619（三色屏）' },
      { value: '04', name: 'SSD1683（黑白屏）' },
      { value: '02', name: 'SSD1619（三色屏）' }
    ]
  },

  // 页面加载
  onLoad: function() {
    this.msgIndex = 0;
    // 初始化防抖定时器
    this.thresholdTimer = null;
    // 蓝牙是否已初始化
    this.bluetoothInitialized = false;
    
    // 确保getImageData方法在旧版本微信上可用
    if (!wx.canIUse('canvas.getImageData')) {
      this.addLog('如果当前微信版本过低，可能不支持画布操作，则需要升级微信版本');
    }
    
    wx.getSystemInfo({
      success: res => {
        // 设置画布尺寸，固定为400x300像素，与电子墨水屏完全匹配
        // 电子墨水屏的分辨率是400x300像素
        this.setData({
          canvasWidth: 400,
          canvasHeight: 300
        });
        
        this.addLog('画布尺寸设置为400x300像素，与墨水屏匹配');
      }
    });
    this.initCanvas();
    
    // 设置取模算法选项
    this.setDitheringOptions();
    
    // 注册蓝牙状态变化回调
    this.registerBluetoothListeners();
  },

  // 页面卸载
  onUnload: function() {
    console.log("页面卸载，清理资源");
    
    // 清理画布资源
    if (this.canvas) {
      this.canvas = null;
      this.ctx = null;
      this.imageInfo = null;
    }
    
    // 停止搜索
    if (this.scanTimer) {
      clearTimeout(this.scanTimer);
      this.scanTimer = null;
    }
    
    // 停止搜索
    if (this.bluetoothInitialized) {
      wx.stopBluetoothDevicesDiscovery({
        complete: () => {
          console.log("停止蓝牙设备搜索");
        }
      });
    }
    
    // 断开连接
    if (this.data.deviceConnected) {
      wx.closeBLEConnection({
        deviceId: this.data.deviceId,
        complete: () => {
          console.log("断开蓝牙连接");
        }
      });
    }
    
    // 移除监听器
    try {
      this.unregisterBluetoothListeners();
      console.log("已移除蓝牙相关监听器");
    } catch (e) {
      console.error("移除蓝牙监听器出错", e);
    }
    
    // 关闭蓝牙适配器
    if (this.bluetoothInitialized) {
      wx.closeBluetoothAdapter({
        success: () => {
          console.log("关闭蓝牙适配器成功");
          this.bluetoothInitialized = false;
        },
        fail: (err) => {
          console.error("关闭蓝牙适配器失败", err);
        }
      });
    }
  },

  // 初始化画布
  initCanvas: function() {
    const query = wx.createSelectorQuery();
    query.select('#canvas').fields({ node: true, size: true }).exec((res) => {
      if (!res || !res[0] || !res[0].node) {
        this.addLog('获取画布节点失败，将在500ms后重试');
        setTimeout(() => this.initCanvas(), 500);
        return;
      }

      const canvas = res[0].node;
      const ctx = canvas.getContext('2d');
      
      // 设置画布的实际宽高，固定为墨水屏分辨率400x300像素
      canvas.width = 400;
      canvas.height = 300;
      
      this.canvas = canvas;
      this.ctx = ctx;
      
      this.addLog(`画布初始化完成，实际尺寸: ${canvas.width}x${canvas.height}`);
      
      // 设置默认背景为白色
      ctx.fillStyle = '#ffffff';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      
      // 如果有图片路径，加载图片
      if (this.data.image) {
        this.loadImage(this.data.image);
      }
    });
  },

  // 添加日志
  addLog: function(text) {
    const logs = this.data.logs;
    const now = new Date();
    const time = now.toLocaleTimeString();
    logs.push({
      time: time,
      text: text
    });
    if (logs.length > 100) logs.shift(); // 限制日志数量
    
    // 计算应该滚动到的位置
    const logScrollTop = 9999; // 一个足够大的数，确保滚动到底部
    
    this.setData({
      logs: logs,
      logScrollTop: logScrollTop
    });
    
    // 记录到全局日志
    util.addLog(text.replace(/<[^>]*>/g, ''));
  },

  // 清除日志
  clearLog: function() {
    this.setData({
      logs: []
    });
  },

  // 注册蓝牙监听器
  registerBluetoothListeners: function() {
    // 监听蓝牙适配器状态变化
    wx.onBluetoothAdapterStateChange(this.handleBluetoothStateChange.bind(this));
    
    // 监听设备发现事件
    wx.onBluetoothDeviceFound(this.handleDeviceFound.bind(this));
    
    // 监听蓝牙连接状态变化
    wx.onBLEConnectionStateChange(this.handleConnectionStateChange.bind(this));
    
    // 监听特征值变化
    wx.onBLECharacteristicValueChange(this.handleCharacteristicValueChange.bind(this));
  },
  
  // 注销蓝牙监听器
  unregisterBluetoothListeners: function() {
    wx.offBluetoothAdapterStateChange();
    wx.offBluetoothDeviceFound();
    wx.offBLEConnectionStateChange();
    wx.offBLECharacteristicValueChange();
  },
  
  // 处理蓝牙状态变化
  handleBluetoothStateChange: function(res) {
    console.log('蓝牙状态变化:', res);
    const { available, discovering } = res;
    
    if (!available && this.bluetoothInitialized) {
      // 蓝牙不可用
      this.addLog('蓝牙适配器不可用');
      this.bluetoothInitialized = false;
      this.setData({
        scanning: false,
        scanText: '扫描'
      });
      
      // 如果正在连接设备，更新状态
      if (this.data.deviceConnected) {
        this.setData({
          deviceConnected: false,
          connected: false
        });
        this.addLog('蓝牙设备已断开');
      }
    }
  },
  
  // 处理设备发现事件
  handleDeviceFound: function(res) {
    if (!this.data.scanning) return;
    
    res.devices.forEach(device => {
      // 检查是否已存在该设备
      const foundIndex = this.data.deviceList.findIndex(item => item.deviceId === device.deviceId);
      if (foundIndex === -1) {
        // 添加新设备
        const newDeviceList = [...this.data.deviceList, {
          deviceId: device.deviceId,
          name: device.name || device.localName || '未命名设备',
          localName: device.localName,
          RSSI: device.RSSI
        }];
        this.setData({
          deviceList: newDeviceList
        });
        this.addLog(`发现设备: ${device.name || device.localName || '未命名设备'}`);
      } else {
        // 更新已存在设备的RSSI
        const updatedDeviceList = [...this.data.deviceList];
        updatedDeviceList[foundIndex].RSSI = device.RSSI;
        this.setData({
          deviceList: updatedDeviceList
        });
      }
    });
  },
  
  // 处理连接状态变化
  handleConnectionStateChange: function(res) {
    const { deviceId, connected } = res;
    
    if (deviceId === this.data.deviceId) {
      if (!connected && this.data.deviceConnected) {
        // 设备断开连接
        this.addLog('设备已断开连接');
        this.setData({
          deviceConnected: false,
          connected: false
        });
      }
    }
  },
  
  // 处理特征值变化
  handleCharacteristicValueChange: function(res) {
    this.handleNotify(res.value);
  },

  // 开始扫描蓝牙设备
  startScan: function() {
    if (this.data.scanning) {
      this.stopScan();
      return;
    }
    
    // 显示加载中，防止页面跳动
    wx.showLoading({
      title: '初始化蓝牙...',
      mask: true // 使用遮罩防止用户点击
    });

    // 检查蓝牙权限
    this.checkBluetoothAuth();
  },
  
  // 检查蓝牙权限
  checkBluetoothAuth: function() {
    wx.getSetting({
      success: (res) => {
        if (res.authSetting['scope.bluetooth']) {
          // 已授权，直接开始扫描
          this.addLog("已获得蓝牙权限，准备扫描");
          this.initBluetoothAdapter();
        } else {
          // 关闭加载提示，显示授权提示
          wx.hideLoading();
          
          // 未授权，先请求授权
          this.addLog("请求蓝牙权限");
          wx.authorize({
            scope: 'scope.bluetooth',
            success: () => {
              this.addLog("蓝牙权限授权成功");
              // 重新显示加载中
              wx.showLoading({
                title: '初始化蓝牙...',
                mask: true
              });
              this.initBluetoothAdapter();
            },
            fail: (err) => {
              this.addLog("蓝牙权限授权失败: " + JSON.stringify(err));
              
              // 引导用户去设置页面开启权限
              wx.showModal({
                title: '需要蓝牙权限',
                content: '请在设置中授权使用蓝牙',
                confirmText: '去设置',
                success: (modalRes) => {
                  if (modalRes.confirm) {
                    wx.openSetting({
                      success: (settingRes) => {
                        if (settingRes.authSetting['scope.bluetooth']) {
                          this.addLog("已在设置中授权蓝牙权限");
                          // 重新显示加载中
                          wx.showLoading({
                            title: '初始化蓝牙...',
                            mask: true
                          });
                          this.initBluetoothAdapter();
                        } else {
                          this.addLog("未授权蓝牙权限，无法扫描");
                          wx.showToast({
                            title: '未授权蓝牙权限',
                            icon: 'none'
                          });
                        }
                      }
                    });
                  }
                }
              });
            }
          });
        }
      },
      fail: (err) => {
        wx.hideLoading();
        this.addLog("获取设置信息失败: " + JSON.stringify(err));
      }
    });
  },
  
  // 初始化蓝牙适配器
  initBluetoothAdapter: function() {
    // 清空设备列表并更新状态
    this.setData({
      deviceList: [],
      scanning: true,
      scanText: '停止'
    });

    // 如果蓝牙已初始化，直接开始扫描
    if (this.bluetoothInitialized) {
      this.startBluetoothDiscovery();
      return;
    }

    this.addLog('正在初始化蓝牙...');
    
    // 先关闭蓝牙适配器，避免"already opened"错误
    wx.closeBluetoothAdapter({
      complete: () => {
        // 延迟执行开启蓝牙，避免一些设备上的问题
        setTimeout(() => {
          this.openBluetoothAdapter();
        }, 300);
      }
    });
  },
  
  // 打开蓝牙适配器
  openBluetoothAdapter: function() {
    wx.openBluetoothAdapter({
      success: (res) => {
        this.addLog('蓝牙适配器初始化成功');
        this.bluetoothInitialized = true;
        
        // 延迟执行扫描，避免一些设备上的问题
        setTimeout(() => {
          this.startBluetoothDiscovery();
        }, 300);
      },
      fail: (err) => {
        this.bluetoothInitialized = false;
        this.setData({
          scanning: false,
          scanText: '扫描'
        });
        
        let errMsg = JSON.stringify(err);
        this.addLog('蓝牙适配器初始化失败: ' + errMsg);
        
        // 处理常见错误
        if (err.errCode === 10001) {
          // 蓝牙未打开
          wx.showModal({
            title: '蓝牙未启用',
            content: '请打开手机蓝牙后重试',
            confirmText: '我知道了',
            showCancel: false
          });
        } else {
          wx.showToast({
            title: '蓝牙初始化失败',
            icon: 'none'
          });
        }
      }
    });
  },
  
  // 开始蓝牙设备发现
  startBluetoothDiscovery: function() {
    // 先确保之前的搜索已停止
    wx.stopBluetoothDevicesDiscovery({
      complete: () => {
        // 开始扫描设备
        this.addLog('开始搜索蓝牙设备...');
        wx.startBluetoothDevicesDiscovery({
          allowDuplicatesKey: false,
          powerLevel: 'high',  // 使用高功率扫描
          success: (res) => {
            // 隐藏加载提示
            wx.hideLoading();
            
            this.addLog('扫描已开始，将在10秒后自动停止');
            
            // 设置定时器，10秒后自动停止扫描
            this.scanTimer = setTimeout(() => {
              if (this.data.scanning) {
                this.stopScan();
                this.addLog('扫描已自动停止');
              }
            }, 10000); // 10秒
          },
          fail: (err) => {
            // 隐藏加载提示
            wx.hideLoading();
            
            this.addLog('扫描失败: ' + JSON.stringify(err));
            this.setData({
              scanning: false,
              scanText: '扫描'
            });
          }
        });
      }
    });
  },
  
  // 停止扫描
  stopScan: function() {
    // 清除定时器
    if (this.scanTimer) {
      clearTimeout(this.scanTimer);
      this.scanTimer = null;
    }
    
    if (!this.bluetoothInitialized) {
      this.setData({
        scanning: false,
        scanText: '扫描'
      });
      return;
    }
    
    wx.stopBluetoothDevicesDiscovery({
      success: () => {
        this.setData({
          scanning: false,
          scanText: '扫描'
        });
        
        // 输出扫描结果
        if (this.data.deviceList.length > 0) {
          this.addLog(`扫描完成，发现 ${this.data.deviceList.length} 个设备`);
        } else {
          this.addLog('扫描完成，未发现设备');
        }
      },
      fail: (err) => {
        this.addLog('停止扫描失败: ' + JSON.stringify(err));
        this.setData({
          scanning: false,
          scanText: '扫描'
        });
      }
    });
  },

  // 连接到设备
  connectDevice: function(e) {
    const deviceId = e.currentTarget.dataset.deviceid;
    const deviceName = e.currentTarget.dataset.name || '未命名设备';
    
    // 显示确认对话框
    wx.showModal({
      title: '连接确认',
      content: `确定要连接到设备"${deviceName}"吗？`,
      confirmText: '连接',
      cancelText: '取消',
      success: (res) => {
        if (!res.confirm) return; // 用户取消
        
        // 停止扫描
        this.stopScan();
        
        this.addLog('正在连接: ' + deviceName);
        
        // 连接设备
        wx.createBLEConnection({
          // connectionPriority: 9999999,
          deviceId: deviceId,
          timeout: 10000,
          success: (res) => {
            this.addLog('连接成功');
            this.setData({
              deviceConnected: true,
              deviceId: deviceId,
              deviceName: deviceName
            });
            
            // 获取服务
            this.getDeviceServices(deviceId);
          },
          fail: (err) => {
            this.addLog('连接失败: ' + JSON.stringify(err));
            wx.showToast({
              title: '连接设备失败',
              icon: 'none'
            });
          }
        });
      }
    });
  },
  
  // 获取设备服务
  getDeviceServices: function(deviceId) {
    this.addLog('正在获取设备服务...');
    
    wx.getBLEDeviceServices({
      deviceId: deviceId,
      success: (res) => {
        this.addLog('获取服务成功');
        
        // 记录所有服务
        const allServices = res.services.map(item => item.uuid);
        this.addLog('发现服务: ' + allServices.join(', '));
        
        // 检查是否找到目标服务
        const targetService = res.services.find(item => 
          item.uuid.toUpperCase().includes(EPD_SERVICE_UUID.slice(0, 8).toUpperCase())
        );
        
        if (!targetService) {
          this.addLog('未找到EPD服务，尝试使用第一个服务');
        }
        
        // 使用找到的服务或第一个服务
        const serviceId = targetService ? targetService.uuid : res.services[0].uuid;
        
        // 获取特征值
        this.getServiceCharacteristics(deviceId, serviceId);
      },
      fail: (err) => {
        this.addLog('获取服务失败: ' + JSON.stringify(err));
        this.disconnectDevice();
      }
    });
  },
  
  // 获取服务特征值
  getServiceCharacteristics: function(deviceId, serviceId) {
    this.addLog('正在获取特征值...');
    
    wx.getBLEDeviceCharacteristics({
      deviceId: deviceId,
      serviceId: serviceId,
      success: (res) => {
        this.addLog('获取特征值成功');
        
        const charList = res.characteristics.map(c => c.uuid);
        this.addLog('特征值列表: ' + charList.join(', '));
        
        // 查找可写的特征值
        const writableChar = res.characteristics.find(c => 
          c.properties.write || c.properties.writeNoResponse
        );
        
        if (!writableChar) {
          this.addLog('未找到可写的特征值');
          wx.showToast({
            title: '设备不支持写入操作',
            icon: 'none',
            duration: 2000
          });
          this.disconnectDevice();
          return;
        }
        
        const characteristicId = writableChar.uuid;
        this.setData({
          currentServiceId: serviceId,
          currentCharacteristicId: characteristicId
        });
        
        // 启用通知
        this.enableNotification(deviceId, serviceId, characteristicId);
      },
      fail: (err) => {
        this.addLog('获取特征值失败: ' + JSON.stringify(err));
        this.disconnectDevice();
      }
    });
  },
  
  // 启用通知
  enableNotification: function(deviceId, serviceId, characteristicId) {
    this.addLog('正在启用通知...');
    
    wx.notifyBLECharacteristicValueChange({
      deviceId: deviceId,
      serviceId: serviceId,
      characteristicId: characteristicId,
      state: true,
      success: (res) => {
        this.addLog('通知启用成功');
        this.setData({
          connected: true
        });
        
        // 初始化设备
        setTimeout(() => {
          this.write(EpdCmd.INIT);
        }, 300);
      },
      fail: (err) => {
        this.addLog('通知启用失败: ' + JSON.stringify(err));
        // 尝试不启用通知继续
        this.setData({
          connected: true
        });
        // 初始化设备
        setTimeout(() => {
          this.write(EpdCmd.INIT);
        }, 300);
      }
    });
  },

  // 断开连接
  disconnectDevice: function() {
    if (!this.data.deviceConnected) return;
    
    // 停止扫描（以防正在扫描）
    this.stopScan();
    
    wx.closeBLEConnection({
      deviceId: this.data.deviceId,
      success: (res) => {
        this.addLog('断开连接成功');
        this.setData({
          connected: false,
          deviceConnected: false,
          deviceName: ''
        });
      },
      fail: (res) => {
        this.addLog('断开连接失败: ' + JSON.stringify(res));
      }
    });
  },

  // 写入特征值
  write: function(cmd, data, withResponse = true) {
    return new Promise((resolve, reject) => {
      if (!this.data.connected || !this.data.deviceId || !this.data.currentServiceId || !this.data.currentCharacteristicId) {
        reject('蓝牙未连接');
        return;
      }
      
      let payload = [cmd];
      if (data) {
        if (typeof data === 'string') {
          data = this.hexToBytes(data);
        }
        if (data instanceof Uint8Array) {
          data = Array.from(data);
        }
        payload = payload.concat(data);
      }
      
      // 记录日志
      this.addLog(`<span style="color:#a00">⇑</span> ${this.bytesToHex(new Uint8Array(payload))}`);
      
      // 若是刷新命令，添加额外日志便于调试
      if (cmd === EpdCmd.REFRESH) {
        this.addLog(`发送刷新命令，参数: ${data ? this.bytesToHex(new Uint8Array(data)) : '无'}`);
      }
      
      // 优化写入方式，根据是否需要响应选择合适的写入方法
      if (withResponse) {
        // 需要响应的写入方式
        wx.writeBLECharacteristicValue({
          deviceId: this.data.deviceId,
          serviceId: this.data.currentServiceId,
          characteristicId: this.data.currentCharacteristicId,
          value: new Uint8Array(payload).buffer,
          success: (res) => {
            resolve(res);
          },
          fail: (err) => {
            this.addLog('写入失败: ' + JSON.stringify(err));
            reject(err);
          }
        });
      } else {
        // 无需响应的写入方式 - 使用无响应模式可以显著加快速度
        wx.writeBLECharacteristicValue({
          deviceId: this.data.deviceId,
          serviceId: this.data.currentServiceId,
          characteristicId: this.data.currentCharacteristicId,
          value: new Uint8Array(payload).buffer,
          // 不等待成功回调即视为完成
          complete: (res) => {
            if (res.errCode) {
              // 出错时记录日志
              this.addLog('写入无响应模式失败: ' + JSON.stringify(res));
              reject(res);
            } else {
              resolve(res);
            }
          }
        });
      }
    });
  },

  // 处理通知
  handleNotify: function(value) {
    const data = new Uint8Array(value);
    if (this.msgIndex === 0) {
      this.addLog(`收到配置：${this.bytesToHex(data)}`);
      if (data.length >= 8) {
        // 确保提取完整的引脚配置信息，包括可选的额外字节
        let pinsHex = this.bytesToHex(data.slice(0, 7));
        if (data.length > 10) {
          pinsHex += ' ' + this.bytesToHex(data.slice(10, 11));
        }
        
        this.setData({
          epdPins: pinsHex,
          epdDriver: this.bytesToHex(data.slice(7, 8))
        });
        
        // 更新驱动选择器索引
        const driverIndex = this.data.driverOptions.findIndex(item => item.value === this.data.epdDriver);
        if (driverIndex !== -1) {
          this.setData({
            epdDriverIndex: driverIndex
          });
        }
        
        // 根据驱动类型过滤可用的取模算法选项
        this.filterDitheringOptions();
      }
    } else {
      const text = this.ab2str(value);
      this.addLog(`<span style="color:#00a">⇓</span> ${text}`);
      
      // 检查是否为刷新完成的确认消息
      if (text.includes('Refresh done') || text.includes('刷新完成')) {
        this.addLog('收到刷新完成确认');
        // 设置刷新完成标志
        this.refreshCompleted = true;
        
        // 清除超时定时器
        if (this.refreshTimeout) {
          clearTimeout(this.refreshTimeout);
          this.refreshTimeout = null;
        }
        
        this.setData({
          statusText: '刷新完成！显示更新成功',
          showStatus: false // 立即隐藏状态栏
        });
      }
    }
    this.msgIndex++;
  },

  // 设置驱动
  setDriver: async function() {
    await this.write(EpdCmd.SET_PINS, this.data.epdPins);
    await this.write(EpdCmd.INIT, this.data.epdDriver);
  },

  // 同步时间（日历/时钟模式）
  syncTime: async function(e) {
    const mode = parseInt(e.currentTarget.dataset.mode);
    const timestamp = Math.floor(Date.now() / 1000);
    const data = new Uint8Array([
      (timestamp >> 24) & 0xFF,
      (timestamp >> 16) & 0xFF,
      (timestamp >> 8) & 0xFF,
      timestamp & 0xFF,
      -new Date().getTimezoneOffset() / 60,
      mode
    ]);
    if (await this.write(EpdCmd.SET_TIME, data)) {
      this.addLog('时间已同步！');
    }
  },

  // 清除屏幕
  clearScreen: async function() {
    const res = await wx.showModal({
      title: '提示',
      content: '确认清除屏幕内容?',
      showCancel: true
    });
    
    if (res.confirm) {
      await this.write(EpdCmd.CLEAR);
    }
  },

  // 清除画布
  clearCanvas: function() {
    if (!this.canvas || !this.ctx) {
      this.initCanvas();
      return;
    }

    wx.showModal({
      title: '提示',
      content: '确认清除画布内容?',
      success: (res) => {
        if (res.confirm) {
          const ctx = this.ctx;
          ctx.fillStyle = '#ffffff';
          ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
          
          // 不清除图像引用对象，保留原始图像信息以便能够重新渲染
          if (this.imageInfo && this.imageInfo.img) {
            // 保存原图引用但标记画布已清除状态
            this.imageInfo.hasBeenCleared = true;
          }
          
          this.addLog('画布已清除，您可以点击"重新渲染"按钮恢复图像');
        }
      }
    });
  },

  // 重新渲染图像
  reprocessImage: function() {
    if (!this.imageInfo || !this.imageInfo.img) {
      wx.showToast({
        title: '没有可用的图像',
        icon: 'none'
      });
      return;
    }

    // 重新绘制原始图像并应用处理
    this.drawImageAndProcess(
      this.imageInfo.img, 
      this.imageInfo.offsetX, 
      this.imageInfo.offsetY, 
      this.imageInfo.drawWidth, 
      this.imageInfo.drawHeight
    );
    
    // 如果之前有清除状态，现在已重新渲染，清除该状态
    if (this.imageInfo.hasBeenCleared) {
      this.imageInfo.hasBeenCleared = false;
    }
    
    this.addLog('图像已重新渲染');
  },

  // 发送命令
  sendCmd: async function() {
    const cmdTXT = this.data.cmdTXT;
    if (cmdTXT === '') return;
    
    const bytes = this.hexToBytes(cmdTXT);
    await this.write(bytes[0], bytes.length > 1 ? bytes.slice(1) : null);
  },

  // 发送图片
  sendImage: async function() {
    if (!this.data.image) {
      wx.showToast({
        title: '请先选择图片',
        icon: 'none'
      });
      return;
    }

    if (!this.data.connected) {
      wx.showToast({
        title: '请先连接蓝牙设备',
        icon: 'none'
      });
      return;
    }

    const driver = this.data.epdDriver;
    const mode = this.data.dithering;

    if (mode === '') {
      wx.showToast({
        title: '请选择一种取模算法',
        icon: 'none'
      });
      return;
    }

    // 固定状态栏显示位置，避免页面跳动
    this.startTime = Date.now();
    this.setData({
      showStatus: true,
      statusText: '准备发送...'
    });

    // 设置刷新状态标志
    this.refreshCompleted = false;
    
    try {
      // 获取画布像素数据
      if (!this.canvas || !this.ctx) {
        throw new Error('画布未初始化');
      }
      
      // 确保使用正确的尺寸获取数据
      const canvasWidth = this.canvas.width;
      const canvasHeight = this.canvas.height;
      
      this.addLog(`画布尺寸: ${canvasWidth}x${canvasHeight}`);
      
      try {
        // 确保获取的是处理后的图像数据
        this.setData({ statusText: '正在读取图像数据...' });
        
        // 获取当前画布上的图像数据（应该是已处理过的）
        const imgData = this.ctx.getImageData(0, 0, canvasWidth, canvasHeight);
        
        this.setData({ statusText: '处理图像...' });
        
        // 根据驱动和模式处理图像并发送
        if (mode.startsWith('bwr')) {
          // 三色屏处理
          const invert = (driver === '02') || (driver === '05');
          
          // 黑白数据 - 与HTML版本对齐命令值
          const bwBytes = imageProcessing.imageDataToBytes(imgData.data, canvasWidth, canvasHeight, 'bw', false);
          this.setData({ statusText: '发送黑白数据...' });
          await this.epdWrite(driver === "02" ? 0x24 : 0x10, bwBytes);
          
          // 红色数据 - 与HTML版本对齐命令值
          const redBytes = imageProcessing.imageDataToBytes(imgData.data, canvasWidth, canvasHeight, 'red', invert);
          this.setData({ statusText: '发送彩色数据...' });
          await this.epdWrite(driver === "02" ? 0x26 : 0x13, redBytes);
        } else {
          // 黑白屏处理 - 与HTML版本对齐命令值
          const bwBytes = imageProcessing.imageDataToBytes(imgData.data, canvasWidth, canvasHeight, 'bw', false);
          this.setData({ statusText: '发送数据...' });
          await this.epdWrite(driver === "04" ? 0x24 : 0x13, bwBytes);
        }
        
        // 刷新显示
        this.setData({ statusText: '正在刷新显示，请勿关闭...' });
        
        // 根据不同的驱动型号添加特定刷新参数
        let refreshParam = null;
        if (driver === '01') { // UC8176（黑白屏）
          refreshParam = [0x01]; // 快速刷新
        } else if (driver === '03' || driver === '05') { // UC8176/UC8276（三色屏）
          refreshParam = [0x01]; // 快速刷新
        } else if (driver === '04' || driver === '02') { // SSD1619/SSD1683
          refreshParam = [0x03]; // 质量模式，但速度更快
        }
        
        // 发送刷新命令，确保等待完成
        await this.write(EpdCmd.REFRESH, refreshParam, true);
        
        // 计算并显示发送时间
        const sendTime = (Date.now() - this.startTime) / 1000.0;
        this.addLog(`指令发送完成！耗时: ${sendTime.toFixed(2)}s，等待屏幕刷新...`);
        this.setData({
          statusText: `指令发送完成！等待墨水屏刷新，可能需要3-5秒...`
        });
        
        // 设置刷新超时检查
        this.refreshTimeout = setTimeout(() => {
          if (!this.refreshCompleted) {
            this.addLog('刷新超时，墨水屏可能已经更新但未收到确认');
            this.refreshCompleted = true; // 标记为已完成，避免重复处理
            this.setData({
              statusText: '刷新可能已完成，但未收到确认',
              showStatus: false // 无论如何，5秒后隐藏状态显示
            });
          }
        }, 5000); // 5秒超时
      } catch (e) {
        throw new Error('获取图像数据失败: ' + e.message);
      }
    } catch (e) {
      console.error('发送图像失败:', e);
      this.addLog('发送图像失败: ' + e.message);
      this.setData({
        showStatus: false
      });
      wx.showToast({
        title: '发送图像失败: ' + e.message,
        icon: 'none',
        duration: 3000
      });
    }
  },

  // 十六进制字符串转字节数组
  hexToBytes: function(hex) {
    if (!hex) return [];
    
    // 移除所有空白字符
    hex = hex.replace(/\s+/g, '');
    
    // 确保十六进制字符串有效
    if (!/^[0-9A-Fa-f]*$/.test(hex)) {
      this.addLog('十六进制字符串无效: ' + hex);
      return [];
    }
    
    // 如果长度不是偶数，补零
    if (hex.length % 2 !== 0) {
      hex = '0' + hex;
    }
    
    // 转换为字节数组
    const bytes = [];
    try {
      for (let i = 0; i < hex.length; i += 2) {
        bytes.push(parseInt(hex.substr(i, 2), 16));
      }
    } catch (e) {
      this.addLog('十六进制转换失败: ' + e.message);
      return [];
    }
    
    return bytes;
  },

  // 字节数组转十六进制字符串
  bytesToHex: function(bytes) {
    if (!bytes || bytes.length === 0) return '';
    
    try {
      return Array.from(bytes).map(b => {
        const hex = (b & 0xFF).toString(16).toUpperCase();
        return hex.length === 1 ? '0' + hex : hex;
      }).join(' ');
    } catch (e) {
      this.addLog('字节转换失败: ' + e.message);
      return '';
    }
  },

  // 发送EPD数据
  epdWrite: async function(cmd, data) {
    if (!this.data.deviceId || !this.data.currentServiceId || !this.data.currentCharacteristicId) {
      this.addLog('蓝牙设备未连接');
      wx.showToast({
        title: '请先连接蓝牙设备',
        icon: 'none'
      });
      return;
    }

    // 参数验证
    const mtuSize = parseInt(this.data.mtuSize);
    if (isNaN(mtuSize) || mtuSize < 1 || mtuSize > 255) {
      this.addLog('MTU大小无效 (范围1-255)');
      wx.showToast({
        title: 'MTU大小无效',
        icon: 'none'
      });
      return;
    }
    
    // 计算有效的块大小 (MTU值 - 1字节命令)
    const chunkSize = mtuSize - 1;
    this.addLog(`使用MTU大小: ${mtuSize}, 块大小: ${chunkSize}字节`);
    
    const interleavedCount = parseInt(this.data.interleavedCount);
    if (isNaN(interleavedCount) || interleavedCount < 1) {
      this.addLog('确认间隔无效');
      wx.showToast({
        title: '确认间隔无效',
        icon: 'none'
      });
      return;
    }
    
    // 确保数据格式正确
    if (typeof data === 'string') {
      data = this.hexToBytes(data);
    }
    
    if (!data || data.length === 0) {
      this.addLog('无数据可发送');
      return;
    }

    try {
      // 发送命令
      await this.write(EpdCmd.SEND_CMD, [cmd]);
      
      // 分块计算
      const count = Math.ceil(data.length / chunkSize);
      let chunkIdx = 0;
      
      // 使用计数器跟踪进度
      let lastProgressUpdate = 0;
      const updateInterval = Math.max(50, Math.floor(count / 20)); // 至少每50个块或5%进度更新一次
      
      // 优化：直接顺序发送，而不是使用Promise.all
      // 这样可以避免创建大量并发请求导致的蓝牙堆栈不稳定
      let confirmInterval = Math.min(interleavedCount, 50); // 限制最大确认间隔
      let confirmedChunks = 0;
      
      while (chunkIdx < count) {
        // 更新当前进度
        const currentTime = (Date.now() - this.startTime) / 1000.0;
        const progress = Math.floor((chunkIdx / count) * 100);
        
        if (chunkIdx - lastProgressUpdate >= updateInterval) {
          this.setData({
            statusText: `命令：0x${cmd.toString(16)}, 进度: ${progress}%, 数据块: ${chunkIdx}/${count}, 用时: ${currentTime.toFixed(1)}s`
          });
          lastProgressUpdate = chunkIdx;
        }
        
        const chunk = data.slice(chunkIdx * chunkSize, (chunkIdx + 1) * chunkSize);
        const needConfirm = (chunkIdx - confirmedChunks >= confirmInterval - 1) || (chunkIdx === count - 1);
        
        if (needConfirm) {
          // 每隔confirmInterval个块或最后一个块需要确认
          await this.write(EpdCmd.SEND_DATA, chunk, true);
          confirmedChunks = chunkIdx + 1;
          
          // 大量数据时每确认点记录一次日志
          if (count > 200 && (chunkIdx % (confirmInterval * 5) === 0 || chunkIdx === count - 1)) {
            this.addLog(`数据块进度: ${chunkIdx+1}/${count} (${progress}%)`);
          }
        } else {
          // 普通数据块无需等待响应
          this.write(EpdCmd.SEND_DATA, chunk, false).catch(err => {
            console.error('无响应模式写入失败:', err);
            // 但我们不中断主流程，继续发送
          });
        }
        
        chunkIdx++;
      }
      
      // 发送完成后显示总用时
      const totalTime = (Date.now() - this.startTime) / 1000.0;
      this.addLog(`数据发送完成，总用时: ${totalTime.toFixed(2)}s，共${count}个数据块`);
      
      return true;
    } catch (e) {
      this.addLog('发送EPD数据失败: ' + e.message);
      throw e;
    }
  },

  // 选择图片
  chooseImage: function() {
    wx.chooseMedia({
      count: 1,
      mediaType: ['image'],
      sizeType: ['original', 'compressed'],
      sourceType: ['album', 'camera'],
      success: (res) => {
        const tempFilePath = res.tempFiles[0].tempFilePath;
        this.setData({
          image: tempFilePath
        });
        this.loadImage(tempFilePath);
      }
    });
  },

  // 绘制图像并应用取模算法
  drawImageAndProcess: function(img, offsetX, offsetY, drawWidth, drawHeight) {
    if (!this.ctx || !this.canvas) return;
    
    // 清空画布
    this.ctx.fillStyle = '#ffffff';
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    
    try {
      // 绘制图片
      this.ctx.drawImage(img, offsetX, offsetY, drawWidth, drawHeight);
      
      // 延迟一点再应用取模算法，确保原始图像已经完全绘制
      setTimeout(() => {
        // 应用取模算法
        this.processImage();
      }, 50);
    } catch (err) {
      wx.hideLoading();
      this.addLog('绘制图片失败: ' + err.message);
      console.error('绘制图片失败:', err);
      wx.showToast({
        title: '绘制图片失败',
        icon: 'none'
      });
    }
  },
  
  // 加载图片到画布
  loadImage: function(filePath) {
    if (!this.canvas) {
      this.initCanvas();
      return;
    }

    // 显示加载提示
    wx.showLoading({
      title: '加载图片中...',
      mask: true
    });

    const ctx = this.ctx;
    const canvas = this.canvas;
    
    // 墨水屏尺寸固定为400x300像素
    const canvasWidth = 400;
    const canvasHeight = 300;

    try {
      // 先检查文件是否存在
      wx.getFileInfo({
        filePath: filePath,
        success: (fileInfo) => {
          // 创建图片对象
          const img = canvas.createImage();
          
          // 添加错误处理
          img.onerror = (err) => {
            wx.hideLoading();
            this.addLog('图片加载失败: ' + (err ? err.toString() : '未知错误'));
            wx.showToast({
              title: '图片加载失败',
              icon: 'none'
            });
          };
          
          img.onload = () => {
            try {
              // 计算图片缩放和位置，保持比例
              const imgRatio = img.width / img.height;
              const canvasRatio = canvasWidth / canvasHeight;
              
              let drawWidth, drawHeight, offsetX, offsetY;
              
              if (imgRatio > canvasRatio) {
                // 图片比画布宽，按宽度缩放
                drawWidth = canvasWidth;
                drawHeight = canvasWidth / imgRatio;
                offsetX = 0;
                offsetY = (canvasHeight - drawHeight) / 2;
              } else {
                // 图片比画布高，按高度缩放
                drawHeight = canvasHeight;
                drawWidth = canvasHeight * imgRatio;
                offsetX = (canvasWidth - drawWidth) / 2;
                offsetY = 0;
              }
              
              // 保存图片尺寸信息，便于后续操作
              this.imageInfo = {
                offsetX: offsetX,
                offsetY: offsetY,
                drawWidth: drawWidth,
                drawHeight: drawHeight,
                img: img
              };
              
              // 绘制图片并处理
              this.drawImageAndProcess(img, offsetX, offsetY, drawWidth, drawHeight);
              
              // 隐藏加载提示
              wx.hideLoading();
            } catch (err) {
              wx.hideLoading();
              this.addLog('绘制图片失败: ' + err.message);
              console.error('绘制图片失败:', err);
              wx.showToast({
                title: '绘制图片失败',
                icon: 'none'
              });
            }
          };
          
          // 设置图片源
          img.src = filePath;
        },
        fail: (err) => {
          wx.hideLoading();
          this.addLog('图片文件不存在或无法访问: ' + JSON.stringify(err));
          wx.showToast({
            title: '图片文件无法访问',
            icon: 'none'
          });
        }
      });
    } catch (err) {
      wx.hideLoading();
      this.addLog('创建图片对象失败: ' + err.message);
      console.error('创建图片对象失败:', err);
      wx.showToast({
        title: '创建图片对象失败',
        icon: 'none'
      });
    }
  },

  // 处理图像
  processImage: function() {
    if (!this.canvas) {
      this.addLog('画布未初始化');
      return;
    }

    // 显示状态提示
    this.setData({
      showStatus: true,
      statusText: '正在处理图像...',
      processingImage: true
    });

    const ctx = this.ctx;
    const dithering = this.data.dithering;
    const threshold = this.data.threshold;
    
    // 使用画布的实际宽高，而不是data中可能不同步的值
    const canvasWidth = this.canvas.width;
    const canvasHeight = this.canvas.height;

    try {
      // 获取原始图像数据
      const imageData = ctx.getImageData(0, 0, canvasWidth, canvasHeight);

      // 使用异步处理以避免阻塞UI
      setTimeout(() => {
        try {
          let processedData;
          
          // 根据不同的取模算法处理图像
          if (dithering.startsWith('bwr_')) {
            // 黑白红模式
            processedData = imageProcessing.ditheringWithPalette(
              imageData.data, 
              canvasWidth, 
              canvasHeight, 
              dithering, 
              imageProcessing.bwrPalette
            );
          } else {
            // 黑白模式
            processedData = imageProcessing.dithering(
              imageData.data, 
              canvasWidth, 
              canvasHeight, 
              threshold, 
              dithering
            );
          }
          
          // 创建新的ImageData对象
          const newImageData = ctx.createImageData(canvasWidth, canvasHeight);
          // 将处理后的数据复制到新的ImageData中
          for (let i = 0; i < processedData.length; i++) {
            newImageData.data[i] = processedData[i];
          }
          
          // 更新画布
          ctx.putImageData(newImageData, 0, 0);
          
          this.setData({
            processingImage: false,
            showStatus: false
          });
          
          this.addLog('图像处理完成');
        } catch (e) {
          this.addLog('处理图像失败: ' + e.message);
          console.error('处理图像失败:', e);
          this.setData({
            processingImage: false,
            showStatus: false
          });
          
          // 如果原始图像信息还在，尝试重绘原始图像
          if (this.imageInfo && this.imageInfo.img) {
            this.addLog('尝试恢复原始图像...');
            this.ctx.fillStyle = '#ffffff';
            this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
            this.ctx.drawImage(
              this.imageInfo.img, 
              this.imageInfo.offsetX, 
              this.imageInfo.offsetY, 
              this.imageInfo.drawWidth, 
              this.imageInfo.drawHeight
            );
          }
          
          wx.showToast({
            title: '图像处理失败',
            icon: 'none'
          });
        }
      }, 50);
    } catch (e) {
      this.addLog('获取图像数据失败: ' + e.message);
      console.error('获取图像数据失败:', e);
      this.setData({
        processingImage: false,
        showStatus: false
      });
      wx.showToast({
        title: '获取图像数据失败',
        icon: 'none'
      });
    }
  },

  // 字符串转ArrayBuffer
  str2ab: function(str) {
    const buf = new ArrayBuffer(str.length);
    const bufView = new Uint8Array(buf);
    for (let i = 0, strLen = str.length; i < strLen; i++) {
      bufView[i] = str.charCodeAt(i);
    }
    return buf;
  },

  // ArrayBuffer转字符串
  ab2str: function(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
  },

  // 处理命令输入
  inputCommand: function(e) {
    this.setData({
      cmdTXT: e.detail.value
    });
  },

  // 处理阈值输入
  inputThreshold: function(e) {
    // 不要即时更新图像，而是等用户输入完成
    const threshold = parseInt(e.detail.value);
    if (!isNaN(threshold) && threshold >= 0 && threshold <= 255) {
      this.setData({
        threshold: threshold
      });
    }
  },
  
  // 阈值输入完成后处理图像
  thresholdBlur: function(e) {
    // 防抖处理
    if (this.thresholdTimer) {
      clearTimeout(this.thresholdTimer);
    }
    
    this.thresholdTimer = setTimeout(() => {
      if (this.imageInfo && this.imageInfo.img && !this.data.dithering.startsWith('bwr_')) {
        // 显示处理中的提示
        this.setData({
          processingImage: true,
          showStatus: true,
          statusText: '正在应用阈值...'
        });
        
        // 使用reprocessImage而不是processImage，确保回到原始图像重新渲染
        setTimeout(() => {
          this.reprocessImage();
        }, 100);
      }
    }, 500);
  },
  
  // 处理MTU输入
  inputMTU: function(e) {
    const mtuSize = parseInt(e.detail.value);
    if (!isNaN(mtuSize)) {
      // 确保MTU在有效范围内
      const validMTU = Math.max(1, Math.min(255, mtuSize));
      // 如果输入为空字符串或0，默认使用20
      const finalMTU = (e.detail.value === '' || mtuSize <= 0) ? 20 : validMTU;
      
      this.setData({
        mtuSize: finalMTU
      });
      
      if (finalMTU !== mtuSize) {
        // 如果调整了值，显示提示
        wx.showToast({
          title: `已调整MTU值为${finalMTU}`,
          icon: 'none',
          duration: 1500
        });
      }
    } else {
      // 无效输入，恢复默认值20
      this.setData({
        mtuSize: 20
      });
    }
  },

  // 处理确认间隔输入
  inputInterleaved: function(e) {
    const interleavedCount = parseInt(e.detail.value);
    if (!isNaN(interleavedCount) && interleavedCount > 0 && interleavedCount <= 500) {
      this.setData({
        interleavedCount: interleavedCount
      });
    }
  },

  // 处理引脚输入
  inputPins: function(e) {
    this.setData({
      epdPins: e.detail.value
    });
  },

  // 处理取模算法选择
  ditheringChange: function(e) {
    const index = e.detail.value;
    const options = this.data.epdDriver === '01' || this.data.epdDriver === '04' 
      ? this.data.bwDitheringOptions 
      : this.data.bwrDitheringOptions;
    
    const dithering = options[index].value;
    
    this.setData({
      dithering: dithering,
      ditheringIndex: index
    });

    // 如果已经有图像，立即重新处理
    if (this.imageInfo && this.imageInfo.img) {
      // 显示处理中的提示
      this.setData({
        processingImage: true,
        showStatus: true,
        statusText: '正在应用取模算法...'
      });
      
      // 使用reprocessImage而不是processImage，确保回到原始图像重新渲染
      setTimeout(() => {
        this.reprocessImage();
      }, 100);
    }
  },

  // 根据驱动类型过滤可用的取模算法选项
  filterDitheringOptions: function() {
    const driver = this.data.epdDriver;
    
    // 根据驱动类型设置合适的取模算法
    if (driver === '01' || driver === '04') {
      // 黑白屏
      if (this.data.dithering.startsWith('bwr_')) {
        // 如果当前是三色取模算法，切换到黑白取模算法
        this.setData({
          dithering: 'none',
          ditheringIndex: 0
        });
      }
    } else {
      // 三色屏
      if (!this.data.dithering.startsWith('bwr_')) {
        // 如果当前是黑白取模算法，切换到三色取模算法
        this.setData({
          dithering: 'bwr_floydsteinberg',
          ditheringIndex: 0
        });
      }
    }
    
    // 如果有图像且已处理过，延迟重新应用取模算法
    if (this.data.image && this.imageInfo && this.imageInfo.img) {
      // 显示处理中的提示
      this.setData({
        processingImage: true,
        showStatus: true,
        statusText: '正在根据驱动类型重新应用取模算法...'
      });
      
      // 延迟处理，确保UI已更新
      setTimeout(() => {
        this.reprocessImage();
      }, 100);
    }
  },

  // 处理驱动选择
  driverChange: function(e) {
    const index = e.detail.value;
    const driver = this.data.driverOptions[index].value;
    
    this.setData({
      epdDriver: driver,
      epdDriverIndex: index
    });
    
    // 切换驱动后重置取模算法选项
    if (driver === '01' || driver === '04') {
      // 黑白屏
      if (this.data.dithering.startsWith('bwr_')) {
        // 如果之前是三色模式，切换到黑白模式需要重置取模算法
        this.setData({
          dithering: 'none',
          ditheringIndex: 0
        });
        
        // 重新处理图像
        if (this.data.image) {
          setTimeout(() => {
            this.processImage();
          }, 100);
        }
      }
    } else {
      // 三色屏
      if (!this.data.dithering.startsWith('bwr_')) {
        // 如果之前是黑白模式，切换到三色模式需要重置取模算法
        this.setData({
          dithering: 'bwr_floydsteinberg',
          ditheringIndex: 0
        });
        
        // 重新处理图像
        if (this.data.image) {
          setTimeout(() => {
            this.processImage();
          }, 100);
        }
      }
    }
  },

  // 设置取模算法选项
  setDitheringOptions: function() {
    this.setData({
      bwDitheringOptions: [
        { value: 'none', name: '二值化' },
        { value: 'bayer', name: 'Bayer抖动' },
        { value: 'floydsteinberg', name: 'Floyd-Steinberg抖动' },
        { value: 'Atkinson', name: 'Atkinson抖动' }
      ],
      bwrDitheringOptions: [
        { value: 'bwr_floydsteinberg', name: '黑白红Floyd-Steinberg' },
        { value: 'bwr_Atkinson', name: '黑白红Atkinson' }
      ]
    });
  },

  // 检查并初始化蓝牙权限
  initBluetoothAuth: function() {
    wx.getSetting({
      success: (res) => {
        if (res.authSetting['scope.bluetooth']) {
          this.addLog('已获得蓝牙权限');
          // 检查蓝牙是否可用
          this.checkBluetoothAvailable();
        } else {
          this.addLog('未获得蓝牙权限，首次扫描时将请求授权');
        }
      },
      fail: (err) => {
        this.addLog('获取授权设置失败: ' + JSON.stringify(err));
      }
    });
  },

  // 检查蓝牙是否可用
  checkBluetoothAvailable: function() {
    wx.getBluetoothAdapterState({
      success: (res) => {
        if (!res.available) {
          this.addLog('蓝牙不可用，请确保打开蓝牙并授予微信权限');
        } else {
          this.addLog('蓝牙已就绪');
        }
      },
      fail: (err) => {
        this.addLog('获取蓝牙状态失败，请检查权限设置');
      }
    });
  },
}); 