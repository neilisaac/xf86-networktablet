
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/input.h>
#include <linux/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86Module.h>
#include <X11/Xatom.h>
#include <xf86_OSproc.h>
#include <xf86.h>
#include <xf86Xinput.h>
#include <exevents.h>
#include <xserver-properties.h>
#include <X11/extensions/XKB.h>
#include <xkbsrv.h>

#include "protocol.h"

typedef struct _TabletDeviceRec
{
	int maxX, maxY, maxPressure;
} TabletDeviceRec, *TabletDevicePtr;

static char tablet_driver_name[] = "networktablet";

static int TabletPreInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags);
static void TabletUnInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags);
static pointer TabletPlug(pointer module, pointer options, int *errmaj, int *errmin);
static void TabletUnplug(pointer p);
static int TabletControl(DeviceIntPtr device, int what);
static int _tablet_init_buttons(DeviceIntPtr device);
static int _tablet_init_axes(DeviceIntPtr device, int firstCall);
static void TabletReadInput(InputInfoPtr pInfo);


_X_EXPORT InputDriverRec TABLET = {
    1,
    tablet_driver_name,
    NULL,
    TabletPreInit,
    TabletUnInit,
    NULL,
    0
};

static XF86ModuleVersionInfo TabletVersionRec =
{
    "networktablet",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    0, 1, 0,			/* version 0.1.0 */
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData networktabletModuleData =
{
    &TabletVersionRec,
    &TabletPlug,
    &TabletUnplug
};



int TabletPreInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags) {
	TabletDevicePtr pTablet;

	pTablet = calloc(1, sizeof(TabletDeviceRec));
	if (!pTablet) {
		pInfo->private = NULL;
		xf86DeleteInput(pInfo, 0);
		return BadAlloc;
	}
	// 128x128 tablet resolution until correct resolution received by device
	pTablet->maxX = pTablet->maxY = 128 -1;
	pTablet->maxPressure = 10000;
	pInfo->private = pTablet;

	pInfo->type_name = XI_TABLET; /* see XI.h */
	pInfo->read_input = TabletReadInput;
	pInfo->switch_mode = NULL;
	pInfo->device_control = TabletControl;

	xf86CollectInputOptions(pInfo, NULL);
	xf86ProcessCommonOptions(pInfo, pInfo->options);

	return Success;
}

void TabletUnInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags){
	TabletDevicePtr pTablet = pInfo->private;
	if (pTablet) {
		free(pTablet);
		pInfo->private = NULL;
	}
	xf86DeleteInput(pInfo, 0);
}


pointer TabletPlug(pointer module, pointer options, int *errmaj, int *errmin)
{
	xf86AddInputDriver(&TABLET, module, 0);
	return module;
}

void TabletUnplug(pointer p)
{
}


int TabletControl(DeviceIntPtr device, int what)
{
	InputInfoPtr pInfo = device->public.devicePrivate;
	struct sockaddr_in addr;
	int ret;

	switch (what) {
	case DEVICE_INIT:
		xf86Msg(X_INFO, "%s: Init\n", pInfo->name);
		if ((ret = _tablet_init_buttons(device)) != Success) return ret;
		if ((ret = _tablet_init_axes(device, TRUE)) != Success) return ret;
		break;
	case DEVICE_ON:
		xf86Msg(X_INFO, "%s: On\n", pInfo->name);
		if (device->public.on) break;

		pInfo->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		bzero(&addr, sizeof(struct sockaddr_in));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(NETWORKTABLET_PORT);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		bind(pInfo->fd, (struct sockaddr *)&addr, sizeof(addr));

		xf86AddEnabledDevice(pInfo);
		device->public.on = TRUE;
		break;
	case DEVICE_OFF:
		xf86Msg(X_INFO, "%s: Off\n", pInfo->name);
		if (!device->public.on) break;
		xf86RemoveEnabledDevice(pInfo);

		close(pInfo->fd);
		pInfo->fd = -1;
		device->public.on = FALSE;
		break;
	}

	return Success;
}


int _tablet_init_buttons(DeviceIntPtr device)
{
	InputInfoPtr pInfo = device->public.devicePrivate;
	Atom *labels = calloc(1, sizeof(Atom));
	CARD8 map[] = { 0, 1 };

	if (!InitButtonClassDeviceStruct(device, 1, labels, map)) {
		xf86Msg(X_ERROR, "%s: Failed to register buttons.\n", pInfo->name);
		free(labels);
		return !Success;
	}

	free(labels);
	return Success;
}

int _tablet_init_axes(DeviceIntPtr device, int firstCall)
{
	InputInfoPtr pInfo = device->public.devicePrivate;
	TabletDevicePtr pTablet = pInfo->private;
	const int num_axes = 3;
	Atom *labels;

	if (firstCall) {
		labels = calloc(num_axes, sizeof(Atom));

		if (!InitValuatorClassDeviceStruct(device,
			num_axes,
			#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
			labels,
			#endif
			GetMotionHistorySize(),
			Absolute)) {
			free(labels);
			return BadAlloc;
		}
		free(labels);
	}

	xf86InitValuatorAxisStruct(device, 0, XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X),
		0, pTablet->maxX,
		1,
		0, 1,
		Absolute);
	xf86InitValuatorAxisStruct(device, 1, XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y),
		0, pTablet->maxY,
		1,
		0, 1,
		Absolute);

	xf86InitValuatorAxisStruct(device, 2, XIGetKnownProperty(AXIS_LABEL_PROP_ABS_PRESSURE),
		0, pTablet->maxPressure,
		1,
		0, 1,
		Absolute);
	
	return Success;
}


void TabletReadInput(InputInfoPtr pInfo)
{
	TabletDevicePtr pTablet = pInfo->private;
	EventPacket event;

	while(xf86WaitForInput(pInfo->fd, 0) > 0) {
		// each EventPacket has at least 7 bytes
		if (recv(pInfo->fd, &event, sizeof(event), 0) >= 7) {
			short x = ntohs(event.x);
			short y = ntohs(event.y);
			short pressure = ntohs(event.pressure);

			switch (event.type) {

			case EVENT_TYPE_MOTION:
				xf86PostMotionEvent(pInfo->dev, TRUE, 0, 3, x, y, pressure);
				break;
			case EVENT_TYPE_BUTTON:
				xf86PostButtonEvent(pInfo->dev, TRUE, event.button, event.down, 0, 3, x, y, pressure);
				break;
			case EVENT_TYPE_SET_RESOLUTION:
				pTablet->maxX = ntohs(event.x);
				pTablet->maxY = ntohs(event.y);
				pTablet->maxPressure = ntohs(event.pressure);
				_tablet_init_axes(pInfo->dev, FALSE);
				// break;
			}
		}
	}
}

