
#define NETWORKTABLET_PORT 40117


#pragma pack(push)
#pragma pack(1)

#define EVENT_TYPE_MOTION    0
#define EVENT_TYPE_BUTTON    1

typedef struct _EventPacket
{
	char type;
	struct {
		short x, y;
		short pressure;
	};
	union {
		struct {
			char button;
			char down;
		};
	};
} EventPacket;

#pragma pack(pop)
