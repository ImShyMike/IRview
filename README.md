# IRview

A tiny IR camera with a screen. It's powered by an ESP32-WROOM-32E with 2mb of PSRAM and 8mb of RAM and uses a Melexis far infrared thermal sensor array with a 32x24 resolution that can measure temperatures from -40 to 300°C and has a 55° FOV and uses a PowerBoost 1000 as a BMS that enables it to use a LiPO battery.

The 320x240px screen at the front is used to display the image data, battery and temperature readings and the buttons are used to switch modes. All pin headers are exposed, even when the case is assembled, to help with debugging. It does not include an usb to UART chip so programming must be done using the exposed TX/RX pins with an external programmer.

---
![Final Render](images/image-9.png)

---

## PCB

![PCB](images/image-10.png)

---

## BOM

| Part      | Qty | Price (USD) | Link |
| --------- | :-: | :---------: | ---- |
|           |  1  |             |      |
|           |  1  |             |      |
|           |  1  |             |      |
|           |  1  |             |      |
|           |  1  |             |      |
|           |  1  |             |      |
|           |  1  |             |      |
|           |  1  |             |      |
| **Total** |     |  ****       |      |
