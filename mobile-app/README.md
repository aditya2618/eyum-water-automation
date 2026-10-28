# iYum Water Automation - Mobile Wrapper App

A dedicated native mobile application for **iYum Water Automation**, wrapping the ESP32 Tank Controller & ESP-01 Relay web dashboard into a smooth, distraction-free native app experience.

---

## 🌟 Features

- **Native App Experience**: Fullscreen WebView without browser URL bars, tabs, or address bars.
- **Connection Header**: Real-time status indicator (🟢 Connected / 🟡 Connecting / 🔴 Offline) and current target IP display.
- **Dynamic Controller Switching**: Easily switch between:
  - Default mDNS: `http://iyum-water.local`
  - Setup AP Mode: `http://192.168.4.1` (when connected to `iYum-Water-Setup` Wi-Fi)
  - Custom Static / Router IP: `http://192.168.1.xxx`
- **Persistent Settings**: The selected controller URL is saved locally via `AsyncStorage` and remembered on every launch.
- **Troubleshooting Screen**: Automatically displays helpful diagnostics and quick-action buttons if connection fails (e.g., if phone is not on the same Wi-Fi network).
- **Hardware Back Navigation**: Android hardware back button navigates internal history smoothly.
- **Dark Mode UI**: Matches the exact GitHub-dark palette (`#0d1117`) of the ESP32 dashboard.

---

## 🚀 How to Run on Your Phone (Expo Go)

### Prerequisites
- Node.js installed on your computer.
- Install the **Expo Go** app on your phone:
  - [Google Play Store (Android)](https://play.google.com/store/apps/details?id=host.exp.exponent)
  - [Apple App Store (iOS)](https://apps.apple.com/app/expo-go/id982107779)

### Steps
1. Open a terminal in the `mobile-app` directory:
   ```bash
   cd mobile-app
   ```
2. Start the Expo development server:
   ```bash
   npm start
   ```
3. A QR code will appear in the terminal:
   - **Android**: Open the **Expo Go** app and tap **"Scan QR code"**.
   - **iOS**: Open the native **Camera** app and scan the QR code, then tap to open in Expo Go.
4. The iYum Water Automation mobile app will load instantly on your device!

> **Note:** Ensure your phone and PC are connected to the same Wi-Fi network (or use `npx expo start --tunnel` if on separate subnets).

---

## 📱 How to Build a Standalone APK for Android

If you want a standalone `.apk` file that you can install directly onto Android devices without Expo Go:

### Option A: Cloud Build using EAS (Recommended)
1. Install EAS CLI:
   ```bash
   npm install -g eas-cli
   ```
2. Log in or create a free Expo account:
   ```bash
   eas login
   ```
3. Configure and build an APK:
   ```bash
   eas build -p android --profile preview
   ```
4. Download the generated `.apk` and install it on your phone!

### Option B: Local Native Build
1. Prebuild native Android project:
   ```bash
   npx expo run:android
   ```

---

## ⚙️ Configuration & Tips

### Connecting to the ESP32 Controller
1. **Home Wi-Fi**:
   - Ensure your phone is connected to your home Wi-Fi network.
   - Open the app. If `http://iyum-water.local` connects, you are all set.
   - *Android Note*: Some Android devices cannot resolve `.local` addresses without a local DNS server. If you get a connection error, tap **"Change Host / IP"** or the ⚙️ Settings icon and type the ESP32's assigned router IP (e.g. `192.168.1.150`).
2. **Initial Setup (Access Point Mode)**:
   - Put ESP32 into AP mode (or boot without configured Wi-Fi).
   - On your phone, connect to Wi-Fi SSID **`iYum-Water-Setup`** (password: `12345678`).
   - In the app, tap **"Switch to AP Mode (192.168.4.1)"**.
   - Configure your home Wi-Fi credentials in the dashboard!
