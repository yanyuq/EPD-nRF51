// logs.js
const util = require('../../utils/util.js')

Page({
  data: {
    logs: []
  },
  onLoad() {
    // 直接从存储中获取日志，不需要再次格式化日期
    this.setData({
      logs: wx.getStorageSync('logs') || []
    })
  },
  
  // 每次进入页面时刷新日志列表
  onShow() {
    this.setData({
      logs: wx.getStorageSync('logs') || []
    })
  },
  
  // 清空全部日志
  clearAllLogs() {
    wx.showModal({
      title: '确认清空',
      content: '确定要清空所有日志记录吗？',
      success: (res) => {
        if (res.confirm) {
          wx.setStorageSync('logs', [])
          this.setData({
            logs: []
          })
          wx.showToast({
            title: '已清空日志',
            icon: 'success'
          })
        }
      }
    })
  }
}) 