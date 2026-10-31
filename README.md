Huawei_E5785_fw_bake
Custom, automated firmware baker for the Huawei E5785 router.

This repository contains the automated GitHub Actions build system that takes the original Huawei firmware, injects custom partitions (System, APP, and WebUI), and generates a fully modified, flash-ready firmware image.

⚠️ IMPORTANT COMPATIBILITY WARNING:
This firmware is ONLY for the E5785Lh-92a (Zain Unlocked) variant.
Do NOT flash this on any other hardware revision or locked model. Flashing the wrong firmware will permanently brick your device.

✨ Custom Modifications Included:
This specific custom build is pre-configured with the following tools and modifications:

🚀 AmneziaWG (Kernel Module) 3.1 – Added for advanced VPN configurations and deep packet inspection bypass at the kernel level.

🛡️ ByeDPI – Included and pre-configured in the startup scripts for bypassing DPI restrictions.

🎨 Modified WebUI – A custom User Interface for better control, monitoring, and management.

❌ Standard WireGuard has been removed from this build to make way for Amnezia.

📥 How to Download the Custom Firmware (No Coding Required)
You don't need to manually run any scripts. GitHub Actions will build the firmware for you automatically.

Click on the "Actions" tab at the top of this repository.

On the left sidebar, click on "Bake Complete E5785 Firmware".

Click the "Run workflow" button on the right side, select the master (or main) branch, and click the green "Run workflow" button.

Wait for the workflow to finish (about 1-2 minutes).

Once finished, you can download the final firmware from two places:

GitHub Releases: Go to the "Releases" section on the right side of the main page and download the latest .bin file.

Artifacts: Click on the completed workflow run, scroll down to the bottom, and download the E5785-Custom-Firmware artifact.



This response is AI-generated, for reference only.
