## 🛠️ Installation Instructions

Follow these steps to set up the development environment. Please adhere to the specific versions listed below to ensure compatibility.

### 1. Install Arduino IDE
Download and install the latest version of the [Arduino IDE](https://www.arduino.cc/en/software).

### 2. Install ESP32 Board Manager
1. Open Arduino IDE and go to **File > Preferences**.
2. In the "Additional Boards Manager URLs" field, add: https://espressif.github.io/arduino-esp32/package_esp32_index.json
3. Go to **Tools > Board > Boards Manager**.
4. Search for **esp32** by **Espressif Systems**.
5. **⚠️ IMPORTANT:** Select **Version 3.0.0** from the dropdown menu and click Install.

### 3. Install EspUsbHost Library
1. Go to **Sketch > Include Library > Manage Libraries**.
2. Search for **EspUsbHost**.
3. Look for the library by **TANAKA**.
4. **⚠️ IMPORTANT:** Select **Version 1.0.1**. Do not install the latest version if it differs.

### 4. Install ESP32-BLE-Mouse (T-vK Fork)
*Note: This library must be installed manually from GitHub to use the correct fork.*

1. Download the library ZIP file from the [T-vK GitHub Repository](https://github.com/T-vK/ESP32-BLE-Mouse).
* Click the green **Code** button > **Download ZIP**.
2. In Arduino IDE, go to **Sketch > Include Library > Add .ZIP Library...**
3. Select the `.zip` file you just downloaded.

### 5. Apply Custom Library Patches
**⚠️ CRITICAL STEP:** You must replace specific library files with the modified versions provided in this repository's `library` folder.

1. Navigate to the `library` folder inside this project repository.
2. Copy the modified files and overwrite the existing files in your Arduino libraries directory (usually located in `Documents\Arduino\libraries`).

**Patch 1: Patch ESP32-BLE-Mouse**
* **Source:** `repo/library/BleMouse.cpp`
* **Destination:** `Documents\Arduino\libraries\ESP32_BLE_Mouse\BleMouse.cpp`

* **Source:** `repo/library/BleConnectionStatus.cpp`
* **Destination:** `Documents\Arduino\libraries\ESP32_BLE_Mouse\BleConnectionStatus.cpp`

**Patch 2: Patch EspUsbHost**
* **Source:** `repo/library/EspUsbHost.h`
* **Destination:** `Documents\Arduino\libraries\EspUsbHost\src\EspUsbHost.h`

* **Source:** `repo/library/EspUsbHost.cpp`
* **Destination:** `Documents\Arduino\libraries\EspUsbHost\src\EspUsbHost.cpp`

> **Note:** If asked, select **"Replace the file in the destination"**.

### 6. Hardware Setup: USB-OTG
Solder the USB-OTG jumper on your ESP32 board.

### 7. Arduino IDE Configuration
Configure the board settings to ensure proper USB communication.

1. Go to **Tools** in the Arduino IDE menu.
2. Ensure the following settings are applied:

| Setting | Value |
| :--- | :--- |
| **USB CDC On Boot** | `Disabled` |
| **USB DFU On Boot** | `Disabled` |
| **Upload Mode** | `UART0 / Hardware CDC` |
| **USB Mode** | `Hardware CDC and JTAG` |

8. Flash the program and run the python GUI application

<img width="1243" height="974" alt="image" src="https://github.com/user-attachments/assets/00ac02eb-1dd8-4f08-a207-63a801fa8325" />

