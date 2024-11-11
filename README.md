# EPD-nRF51

4.2 寸电子墨水屏固件，带有一个网页版上位机，可以通过蓝牙传输图像到墨水屏。

理论上支持所有 nRF51 系列 MCU，内置 3 个微雪 4.2 寸墨水屏驱动（可切换），同时还支持自定义墨水屏到 MCU 的引脚映射。

![](docs/images/0.jpg)

## 支持设备

- 老五 4.2 寸价签，黑白双色版本

	```
	MCU：nRF51822
	RAM：16K
	ROM：128K

	驱动：EPD_4in2
	引脚映射：0508090A0B0C0D
	```

	![](docs/images/1.jpg)

- 老五 4.2 寸价签，黑白红三色版本

	```
	MCU：nRF51802
	RAM：16K
	ROM：256K

	驱动：EPD_4in2b_V2
	引脚映射：0A0B0C0D0E0F10
	```

## 致谢

- 屏幕驱动代码来自微雪 [E-Paper Shield](https://www.waveshare.net/wiki/E-Paper_Shield)
- 网页版上位机代码来自 [atc1441/ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper)