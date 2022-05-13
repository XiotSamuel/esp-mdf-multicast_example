# Mwifi Multicasing Example

## Introduction

This example demonstrates how to multicast data to the devices identified by their group ID (not by MAC address), based on `Mwifi` module APIs. Each device has predefined role (READER or WRITER), WRITER only transmits data to specified groups, READER receives the data and prints it to standard output. The group IDs are hard-coded, see the comments in main/mwifi_example.c for implementation details.

## Hardware

* At least 2 x ESP32 development boards
* 1 x router that supports 2.4G

## Process

### Configure the devices

Enter `make menuconfig`, and configure the followings under the submenu "Example Configuration".

	* The router information: If you cannot get the router's channel, please set it as 0, which indicates that the channel will be automatically acquired by the devices.
	* ESP-MESH network: set the same MeshID for all devices within one mesh network
	* Device transmission role: set READER/WRITER here
	* ESP-Mesh node type: keep default ('Device type unset (autocfg by network)')

### Build and Flash

```shell
make erase_flash flash -j5 monitor ESPBAUD=921600 ESPPORT=/dev/ttyUSB0
```

### Run

WRITER device multicasts messages as follows:
```shell
D (1033944) [mwifi_examples, 149]: WRITER message sent: size 21, data {"seq":478,"layer":1}
W (1033949) [mwifi_examples, 155]:      sent to group_id: 01:00:5e:ae:ae:ae
W (1033992) [mwifi_examples, 155]:      sent to group_id: 01:00:5e:aa:aa:aa
W (1033993) [mwifi_examples, 155]:      sent to group_id: 01:00:5e:ac:ac:ac
```

READER devices receives the data:
```shell
D (417953) [mwifi_examples, 71]: READER message received from d8:a0:1d:64:c1:5c, size: 21, data: {"seq":478,"layer":1}
```



