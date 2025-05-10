/**
 * 图像处理工具库
 * 为微信小程序提供图像处理和取模算法
 */

// 黑白红三色调色板
const bwrPalette = [
  [0, 0, 0, 255],     // 黑色
  [255, 255, 255, 255], // 白色
  [255, 0, 0, 255]    // 红色
];

// 黑白双色调色板
const bwPalette = [
  [0, 0, 0, 255],     // 黑色
  [255, 255, 255, 255]  // 白色
];

/**
 * 处理图像数据应用指定的取模算法
 * @param {Uint8ClampedArray} imageData 图像数据
 * @param {number} width 图像宽度
 * @param {number} height 图像高度
 * @param {number} threshold 阈值
 * @param {string} type 取模算法类型
 * @return {Uint8ClampedArray} 处理后的图像数据
 */
function dithering(imageData, width, height, threshold, type) {
  // 拷贝数据，避免直接修改原始数据
  const data = new Uint8ClampedArray(imageData);
  const dataLength = data.length;
  
  // Bayer阈值矩阵
  const bayerThresholdMap = [
    [  15, 135,  45, 165 ],
    [ 195,  75, 225, 105 ],
    [  60, 180,  30, 150 ],
    [ 240, 120, 210,  90 ]
  ];

  // 亮度权重
  const lumR = 0.299;
  const lumG = 0.587;
  const lumB = 0.114;

  // 先将图像转成灰度
  for (let i = 0; i < dataLength; i += 4) {
    const gray = Math.floor(data[i] * lumR + data[i+1] * lumG + data[i+2] * lumB);
    data[i] = gray;
  }

  let newPixel, err;

  // 根据不同的取模算法处理图像
  for (let currentPixel = 0; currentPixel < dataLength; currentPixel += 4) {
    if (type === "gray") {
      // 灰度
      const factor = 255 / (threshold - 1);
      data[currentPixel] = Math.round(data[currentPixel] / factor) * factor;
    } else if (type === "none") {
      // 简单二值化
      data[currentPixel] = data[currentPixel] < threshold ? 0 : 255;
    } else if (type === "bayer") {
      // 4x4 Bayer有序抖动
      const x = Math.floor((currentPixel / 4) % width);
      const y = Math.floor((currentPixel / 4) / width);
      const map = Math.floor((data[currentPixel] + bayerThresholdMap[x % 4][y % 4]) / 2);
      data[currentPixel] = (map < threshold) ? 0 : 255;
    } else if (type === "floydsteinberg") {
      // Floyd-Steinberg 抖动算法
      newPixel = data[currentPixel] < threshold ? 0 : 255;
      err = Math.floor((data[currentPixel] - newPixel) / 16);
      data[currentPixel] = newPixel;

      // 确保不会越界
      if (currentPixel + 4 < dataLength && (currentPixel / 4 + 1) % width !== 0) 
        data[currentPixel + 4] += err * 7;
      if (currentPixel + 4 * width - 4 < dataLength && (currentPixel / 4) % width !== 0) 
        data[currentPixel + 4 * width - 4] += err * 3;
      if (currentPixel + 4 * width < dataLength) 
        data[currentPixel + 4 * width] += err * 5;
      if (currentPixel + 4 * width + 4 < dataLength && (currentPixel / 4 + 1) % width !== 0) 
        data[currentPixel + 4 * width + 4] += err * 1;
    } else if (type === "Atkinson") {
      // Bill Atkinson算法
      newPixel = data[currentPixel] < threshold ? 0 : 255;
      err = Math.floor((data[currentPixel] - newPixel) / 8);
      data[currentPixel] = newPixel;

      // 确保不会越界
      if (currentPixel + 4 < dataLength && (currentPixel / 4 + 1) % width !== 0) 
        data[currentPixel + 4] += err;
      if (currentPixel + 8 < dataLength && (currentPixel / 4 + 2) % width !== 0 && (currentPixel / 4 + 1) % width !== 0) 
        data[currentPixel + 8] += err;
      if (currentPixel + 4 * width - 4 < dataLength && (currentPixel / 4) % width !== 0) 
        data[currentPixel + 4 * width - 4] += err;
      if (currentPixel + 4 * width < dataLength) 
        data[currentPixel + 4 * width] += err;
      if (currentPixel + 4 * width + 4 < dataLength && (currentPixel / 4 + 1) % width !== 0) 
        data[currentPixel + 4 * width + 4] += err;
      if (currentPixel + 8 * width < dataLength) 
        data[currentPixel + 8 * width] += err;
    } else {
      // 默认使用二值化
      data[currentPixel] = data[currentPixel] < threshold ? 0 : 255;
    }

    // 将g和b通道设置为和r通道一样
    data[currentPixel + 1] = data[currentPixel];
    data[currentPixel + 2] = data[currentPixel];
  }

  return data;
}

/**
 * 计算两个颜色的距离
 * @param {Array} rgba1 第一个颜色RGBA值
 * @param {Array} rgba2 第二个颜色RGBA值
 * @return {number} 颜色距离
 */
function getColorDistance(rgba1, rgba2) {
  const [r1, g1, b1] = rgba1;
  const [r2, g2, b2] = rgba2;

  const rm = (r1 + r2) / 2;
  const r = r1 - r2;
  const g = g1 - g2;
  const b = b1 - b2;

  return Math.sqrt((2 + rm / 256) * r * r + 4 * g * g + (2 + (255 - rm) / 256) * b * b);
}

/**
 * 找到最接近的颜色
 * @param {Array} color 目标颜色RGBA值
 * @param {Array} palette 调色板
 * @return {Array} 最接近的颜色
 */
function getNearColorV2(color, palette) {
  let minDistanceSquared = 255 * 255 + 255 * 255 + 255 * 255 + 1;

  let bestIndex = 0;
  for (let i = 0; i < palette.length; i++) {
    let rdiff = (color[0] & 0xff) - (palette[i][0] & 0xff);
    let gdiff = (color[1] & 0xff) - (palette[i][1] & 0xff);
    let bdiff = (color[2] & 0xff) - (palette[i][2] & 0xff);
    let distanceSquared = rdiff * rdiff + gdiff * gdiff + bdiff * bdiff;
    if (distanceSquared < minDistanceSquared) {
      minDistanceSquared = distanceSquared;
      bestIndex = i;
    }
  }
  return palette[bestIndex];
}

/**
 * 更新像素颜色
 * @param {Uint8ClampedArray} imageData 图像数据
 * @param {number} index 像素索引
 * @param {Array} color 新颜色RGBA值
 */
function updatePixel(imageData, index, color) {
  if (index + 3 < imageData.length) {
    imageData[index] = color[0];
    imageData[index + 1] = color[1];
    imageData[index + 2] = color[2];
    imageData[index + 3] = color[3];
  }
}

/**
 * 计算颜色误差
 * @param {Array} color1 原始颜色
 * @param {Array} color2 新颜色
 * @param {number} rate 误差率
 * @return {Array} 误差值
 */
function getColorErr(color1, color2, rate) {
  const res = [];
  for (let i = 0; i < 3; i++) {
    res.push(Math.floor((color1[i] - color2[i]) / rate));
  }
  return res;
}

/**
 * 根据误差更新像素
 * @param {Uint8ClampedArray} imageData 图像数据
 * @param {number} index 像素索引
 * @param {Array} err 误差值
 * @param {number} rate 误差率
 */
function updatePixelErr(imageData, index, err, rate) {
  if (index + 2 < imageData.length) {
    imageData[index] = Math.max(0, Math.min(255, imageData[index] + err[0] * rate));
    imageData[index + 1] = Math.max(0, Math.min(255, imageData[index + 1] + err[1] * rate));
    imageData[index + 2] = Math.max(0, Math.min(255, imageData[index + 2] + err[2] * rate));
  }
}

/**
 * 三色抖动处理
 * @param {Uint8ClampedArray} imageData 图像数据
 * @param {number} width 图像宽度
 * @param {number} height 图像高度
 * @param {string} type 抖动类型
 * @param {Array} palette 调色板
 * @return {Uint8ClampedArray} 处理后的图像数据
 */
function ditheringWithPalette(imageData, width, height, type, palette) {
  palette = palette || bwrPalette;
  
  // 拷贝数据，避免直接修改原始数据
  const data = new Uint8ClampedArray(imageData);
  const dataLength = data.length;
  
  for (let currentPixel = 0; currentPixel < dataLength; currentPixel += 4) {
    // 跳过完全透明的像素
    if (data[currentPixel + 3] === 0) continue;
    
    // 获取当前像素
    const currentColor = [data[currentPixel], data[currentPixel + 1], data[currentPixel + 2], data[currentPixel + 3]];
    
    // 找到最接近的颜色
    const newColor = getNearColorV2(currentColor, palette);
    
    if (type === "bwr_floydsteinberg") {
      // Floyd-Steinberg 抖动算法
      const err = getColorErr(currentColor, newColor, 16);
      
      updatePixel(data, currentPixel, newColor);
      
      // 确保不会越界
      if (currentPixel + 4 < dataLength && (currentPixel / 4 + 1) % width !== 0) 
        updatePixelErr(data, currentPixel + 4, err, 7);
      if (currentPixel + 4 * width - 4 < dataLength && (currentPixel / 4) % width !== 0) 
        updatePixelErr(data, currentPixel + 4 * width - 4, err, 3);
      if (currentPixel + 4 * width < dataLength) 
        updatePixelErr(data, currentPixel + 4 * width, err, 5);
      if (currentPixel + 4 * width + 4 < dataLength && (currentPixel / 4 + 1) % width !== 0) 
        updatePixelErr(data, currentPixel + 4 * width + 4, err, 1);
    } else if (type === "bwr_Atkinson") {
      // Atkinson抖动算法
      const err = getColorErr(currentColor, newColor, 8);
      
      updatePixel(data, currentPixel, newColor);
      
      // 确保不会越界
      if (currentPixel + 4 < dataLength && (currentPixel / 4 + 1) % width !== 0) 
        updatePixelErr(data, currentPixel + 4, err, 1);
      if (currentPixel + 8 < dataLength && (currentPixel / 4 + 2) % width !== 0 && (currentPixel / 4 + 1) % width !== 0) 
        updatePixelErr(data, currentPixel + 8, err, 1);
      if (currentPixel + 4 * width - 4 < dataLength && (currentPixel / 4) % width !== 0) 
        updatePixelErr(data, currentPixel + 4 * width - 4, err, 1);
      if (currentPixel + 4 * width < dataLength) 
        updatePixelErr(data, currentPixel + 4 * width, err, 1);
      if (currentPixel + 4 * width + 4 < dataLength && (currentPixel / 4 + 1) % width !== 0) 
        updatePixelErr(data, currentPixel + 4 * width + 4, err, 1);
      if (currentPixel + 8 * width < dataLength) 
        updatePixelErr(data, currentPixel + 8 * width, err, 1);
    } else {
      // 无抖动，直接替换颜色
      updatePixel(data, currentPixel, newColor);
    }
  }
  
  return data;
}

/**
 * 将图像数据转换为发送到墨水屏的字节数组
 * @param {Uint8ClampedArray} imageData 图像数据
 * @param {number} width 图像宽度
 * @param {number} height 图像高度
 * @param {string} type 类型 'bw' 或 'red'
 * @param {boolean} invert 是否反色
 * @return {Array} 字节数组
 */
function imageDataToBytes(imageData, width, height, type = 'bw', invert = false) {
  const arr = [];
  let buffer = [];
  
  // 注意：这里一定要按照原始HTML版本的顺序来遍历像素
  // 先按行遍历，然后按列，确保字节顺序一致
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const i = (width * y + x) * 4;
      
      if (type !== 'red') {
        // 黑白模式：白色为1，黑色为0
        // 与HTML版本保持一致：imageData.data[i] === 0 ... ? 0 : 1
        buffer.push(imageData[i] === 0 && imageData[i+1] === 0 && imageData[i+2] === 0 ? 0 : 1);
      } else {
        // 红色模式：红色为0，其他为1
        // 与HTML版本保持一致：imageData.data[i] > 0 && imageData.data[i+1] === 0 ... ? 0 : 1
        buffer.push(imageData[i] > 0 && imageData[i+1] === 0 && imageData[i+2] === 0 ? 0 : 1);
      }
      
      // 每8个像素构成一个字节
      if (buffer.length === 8) {
        // 转换为字节，与HTML版本完全一致
        const data = parseInt(buffer.join(''), 2);
        arr.push(invert ? ~data & 0xFF : data); // 取反并限制在0-255范围内
        buffer = [];
      }
    }
  }
  
  // 处理剩余的像素（如果有的话）
  // 这段代码在原版中不存在，但我们保留它以处理非8的倍数宽度的屏幕
  if (buffer.length > 0) {
    // 填充剩余位为1，与HTML版本行为一致
    while (buffer.length < 8) buffer.push(1);
    const data = parseInt(buffer.join(''), 2);
    arr.push(invert ? ~data & 0xFF : data);
  }
  
  return arr;
}

/**
 * 处理Canvas数据
 * @param {Uint8ClampedArray} imageData 图像数据
 * @param {number} width 图像宽度
 * @param {number} height 图像高度
 * @param {Function} processor 处理函数
 * @return {Uint8ClampedArray} 处理后的图像数据
 */
function processCanvasData(imageData, width, height, processor) {
  if (typeof processor !== 'function') {
    return imageData;
  }
  
  try {
    return processor(imageData, width, height);
  } catch (e) {
    console.error('处理图像数据时出错:', e);
    return imageData;
  }
}

/**
 * 双缓冲区的图像处理，避免阻塞UI
 * @param {Uint8ClampedArray} imageData 图像数据
 * @param {number} width 图像宽度
 * @param {number} height 图像高度
 * @param {Function} processor 处理函数
 * @param {Function} callback 完成回调
 */
function processCanvasDataAsync(imageData, width, height, processor, callback) {
  setTimeout(() => {
    try {
      const processedData = processor(imageData, width, height);
      callback(null, processedData);
    } catch (e) {
      console.error('异步处理图像数据时出错:', e);
      callback(e, imageData);
    }
  }, 0);
}

module.exports = {
  dithering: dithering,
  ditheringWithPalette: ditheringWithPalette,
  imageDataToBytes: imageDataToBytes,
  processCanvasData: processCanvasData,
  processCanvasDataAsync: processCanvasDataAsync,
  bwPalette: bwPalette,
  bwrPalette: bwrPalette
}; 