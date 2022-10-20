/* SANA-II prototypes and defines */

#include <devices/sana2.h>

void S2DeviceQuery(struct UnitData *ud, struct IOSana2Req *req);
void S2GetStationAddress(struct UnitData *ud, struct IOSana2Req *req);
void S2ConfigInterface(struct UnitData *ud, struct IOSana2Req *req);
void S2Online(struct UnitData *ud, struct IOSana2Req *req);
void S2Offline(struct UnitData *ud, struct IOSana2Req *req);
void S2GetGlobalStats(struct UnitData *ud, struct IOSana2Req *req);

