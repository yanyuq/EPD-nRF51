App({
  onLaunch: function() {
    // 小程序启动时执行的逻辑
    console.log('小程序启动');
  },
  globalData: {
    // 全局数据
    bleDevice: null,
    gattServer: null,
    epdService: null,
    epdCharacteristic: null,
    connected: false
  }
})