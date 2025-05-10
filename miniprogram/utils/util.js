/**
 * 格式化时间
 * @param {Date} date 
 * @returns {string} 格式化后的时间字符串
 */
const formatTime = date => {
  const year = date.getFullYear()
  const month = date.getMonth() + 1
  const day = date.getDate()
  const hour = date.getHours()
  const minute = date.getMinutes()
  const second = date.getSeconds()

  return `${[year, month, day].map(formatNumber).join('/')} ${[hour, minute, second].map(formatNumber).join(':')}`
}

/**
 * 格式化数字
 * @param {number} n 
 * @returns {string} 格式化后的数字字符串
 */
const formatNumber = n => {
  n = n.toString()
  return n[1] ? n : `0${n}`
}

/**
 * 向本地存储中添加日志
 * @param {string} message 日志消息
 */
const addLog = message => {
  try {
    const logs = wx.getStorageSync('logs') || [];
    // 存储日志时间和消息
    const now = new Date();
    
    // 确保日期对象有效
    if (isNaN(now.getTime())) {
      console.error('Invalid date object in addLog');
      // 使用当前时间戳创建新的日期对象
      now = new Date(Date.now());
    }
    
    // 使用try-catch进行格式化，防止出错
    let formattedDate;
    try {
      formattedDate = formatTime(now);
    } catch (e) {
      console.error('Error formatting date:', e);
      formattedDate = now.toISOString();
    }
    
    logs.unshift({
      timestamp: now.getTime(), // 存储毫秒时间戳
      date: formattedDate,      // 存储格式化的日期
      message: message
    });
    
    // 只保留最新的200条日志
    if (logs.length > 200) {
      logs.splice(200);
    }
    
    wx.setStorageSync('logs', logs);
  } catch (e) {
    console.error('Error in addLog:', e);
  }
}

module.exports = {
  formatTime,
  formatNumber,
  addLog
} 