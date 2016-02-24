
1. To build get into directory reset_usb_device and run build_reset_usb_device.sh script.

2. Run lsusb to get the device path for example after connecting cp2102  here is the output

Bus 001 Device 002: ID 8087:8008 Intel Corp. 
Bus 002 Device 002: ID 8087:8000 Intel Corp. 
Bus 003 Device 002: ID 174f:1474 Syntek 
Bus 003 Device 025: ID 10c4:ea60 Cygnal Integrated Products, Inc. CP210x Composite Device
Bus 003 Device 024: ID 04ca:0061 Lite-On Technology Corp. 
Bus 003 Device 004: ID 105b:e065  
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 002 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 003 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 004 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub

3. Run executable reset_usb_device as root user as shown below :

reset_usb_device /dev/bus/usb/003/025

This will reset CP2102 prgrammatically as can be verified from dmesg log :

[25576.225944] usb 3-3: reset full-speed USB device number 25 using xhci_hcd
[25576.242608] xhci_hcd 0000:00:14.0: xHCI xhci_drop_endpoint called with disabled ep ffff88022ee9eb40
[25576.242611] xhci_hcd 0000:00:14.0: xHCI xhci_drop_endpoint called with disabled ep ffff88022ee9eb00
[25576.243191] usb 3-3: cp210x converter now attached to ttyUSB0

