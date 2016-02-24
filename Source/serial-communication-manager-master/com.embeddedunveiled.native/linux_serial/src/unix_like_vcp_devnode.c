/***************************************************************************************************
 * Author : Rishi Gupta
 *
 * This file is part of 'serial communication manager' library.
 *
 * The 'serial communication manager' is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * The 'serial communication manager' is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with serial communication manager. If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#if defined (__linux__)
#include <sys/types.h>
#include <libudev.h>
#endif

#if defined (__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBLib.h>
#endif

#include <jni.h>
#include "unix_like_serial_lib.h"

#if defined (__linux__)
/*
 * Find device nodes (like ttyS1, ttyUSB0) assigned by operating system to the USB-UART bridge/converter(s)
 * from the USB device attributes.
 *
 * The USB strings are Unicode, UCS2 encoded, but the strings returned from udev_device_get_sysattr_value()
 * are UTF-8 encoded. GetStringUTFChars() returns in modified UTF-8 encoding.
 *
 * Returns array of name if assigned name found, empty array if not found, NULL if an error occurs.
 */
jobjectArray vcp_node_from_usb_attributes(JNIEnv *env, jint usbvid_to_match, jint usbpid_to_match,
		jstring serial_number) {

	int x = 0;
	struct jstrarray_list list = {0};
	jclass strClass = NULL;
	jobjectArray vcpPortsFound = NULL;
	const char* serial_to_match = NULL;
	jstring vcp_node;

	struct udev *udev_ctx;
	struct udev_enumerate *enumerator;
	struct udev_list_entry *devices, *dev_list_entry;
	const char *prop_val;
	const char *path;
	struct udev_device *udev_device;
	int usb_vid;
	int usb_pid;
	char *endptr;

	init_jstrarraylist(&list, 50);

	if(serial_number != NULL) {
		serial_to_match = (*env)->GetStringUTFChars(env, serial_number, NULL);
		if((serial_to_match == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
			free_jstrarraylist(&list);
			throw_serialcom_exception(env, 3, 0, E_GETSTRUTFCHARSTR);
			return NULL;
		}
	}

	udev_ctx = udev_new();
	enumerator = udev_enumerate_new(udev_ctx);
	udev_enumerate_add_match_subsystem(enumerator, "tty");
	udev_enumerate_scan_devices(enumerator);
	devices = udev_enumerate_get_list_entry(enumerator);

	udev_list_entry_foreach(dev_list_entry, devices) {
		path = udev_list_entry_get_name(dev_list_entry);
		udev_device = udev_device_new_from_syspath(udev_enumerate_get_udev(enumerator), path);
		if(udev_device == NULL) {
			continue;
		}

		/* match vid */
		prop_val = udev_device_get_property_value(udev_device, "ID_VENDOR_ID");
		if(prop_val != NULL) {
			usb_vid = 0x0000FFFF & (int) strtol(prop_val, &endptr, 16);
			if(usb_vid != usbvid_to_match) {
				udev_device_unref(udev_device);
				continue;
			}
		}else {
			udev_device_unref(udev_device);
			continue;
		}

		/* match pid */
		prop_val = udev_device_get_property_value(udev_device, "ID_MODEL_ID");
		if(prop_val != NULL) {
			usb_pid = 0x0000FFFF & (int) strtol(prop_val, &endptr, 16);
			if(usb_pid != usbpid_to_match) {
				udev_device_unref(udev_device);
				continue;
			}
		}else {
			udev_device_unref(udev_device);
			continue;
		}

		/* match serial if required by application */
		if(serial_to_match != NULL) {
			prop_val = udev_device_get_property_value(udev_device, "ID_SERIAL_SHORT");
			if(prop_val != NULL) {
				if(strcasecmp(prop_val, serial_to_match) != 0) {
					udev_device_unref(udev_device);
					continue;
				}
			}else {
				udev_device_unref(udev_device);
				continue;
			}
		}

		/* reaching here means that this device meets all criteria, so get its device node */
		prop_val = udev_device_get_property_value(udev_device, "DEVNAME");
		if(prop_val != NULL) {
			vcp_node = (*env)->NewStringUTF(env, prop_val);
			if((vcp_node == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
				throw_serialcom_exception(env, 3, 0, E_NEWSTRUTFSTR);
				return NULL;
			}
			insert_jstrarraylist(&list, vcp_node);
		}

		udev_device_unref(udev_device);
	}

	/* release resources */
	udev_enumerate_unref(enumerator);
	udev_unref(udev_ctx);
	(*env)->ReleaseStringUTFChars(env, serial_number, serial_to_match);

	/* create a JAVA/JNI style array of String object, populate it and return to java layer. */
	strClass = (*env)->FindClass(env, JAVALSTRING);
	if((strClass == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		(*env)->ExceptionClear(env);
		free_jstrarraylist(&list);
		throw_serialcom_exception(env, 3, 0, E_FINDCLASSSSTRINGSTR);
		return NULL;
	}

	vcpPortsFound = (*env)->NewObjectArray(env, (jsize) list.index, strClass, NULL);
	if((vcpPortsFound == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		(*env)->ExceptionClear(env);
		free_jstrarraylist(&list);
		throw_serialcom_exception(env, 3, 0, E_NEWOBJECTARRAYSTR);
		return NULL;
	}

	for (x=0; x < list.index; x++) {
		(*env)->SetObjectArrayElement(env, vcpPortsFound, x, list.base[x]);
		if((*env)->ExceptionOccurred(env)) {
			(*env)->ExceptionClear(env);
			free_jstrarraylist(&list);
			throw_serialcom_exception(env, 3, 0, E_SETOBJECTARRAYSTR);
			return NULL;
		}
	}

	free_jstrarraylist(&list);
	return vcpPortsFound;
}
#endif

#if defined (__APPLE__)
/*
 * Find device nodes (like ttyS1, ttyUSB0) assigned by operating system to the USB-UART bridge/converter(s)
 * from the USB device attributes.
 *
 * The USB strings are Unicode, UCS2 encoded, but the strings returned from udev_device_get_sysattr_value() are
 * UTF-8 encoded. GetStringUTFChars() returns in modified UTF-8 encoding.
 */
jobjectArray vcp_node_from_usb_attributes(JNIEnv *env, jint usbvid_to_match, jint usbpid_to_match,
		jstring serial_number) {

	int x = 0;
	struct jstrarray_list list = {0};
	jclass strClass = NULL;
	jobjectArray vcpPortsFound = NULL;
	const char* serial_to_match = NULL;
	jstring vcp_node;
}
#endif
