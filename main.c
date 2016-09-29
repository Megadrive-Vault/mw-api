/************************************************************************//**
 * \brief MeGaWiFi API test program. Uses SGDK to ease Genesis/Megadrive
 *        console programming.
 *
 * \author Jesus Alonso (doragasu)
 ****************************************************************************/

#include "mw/megawifi.h"
// Remove this when 16c550 driver is complete, it should be included only
// by megawifi module.
#include "mw/16c550.h"
#include "mw/lsd.h"
#include "mw/util.h"
#include "ssid_config.h"
// SGDK includes must be after mw ones, or they will conflict with stdint.h
#include <genesis.h>

#define TCP_TEST_CH		1

#define dtext(str, col)	do{VDP_drawText(str, col, line++);\
                           if ((line) > 28)line = 0;}while(0)

#define IPV4_BUILD(a, b, c, d)	(((a)<<24) | ((b)<<16) | ((c)<<8) | (d))

static inline void DelayFrames(unsigned int fr) {
	while (fr--) VDP_waitVSync();
}

static const char echoTestStr[] = "OLAKASE, ECO O KASE!";

static const char spinner[] = "|/-\\";
static const char hexTable[] = "0123456789ABCDEF";
static unsigned char line = 0;

// Command to send
static MwCmd cmd;
// Command reply
static MwCmd rep;

static inline void ByteToHexStr(uint8_t byte, char hexStr[]){
	hexStr[0] = hexTable[byte>>4];
	hexStr[1] = hexTable[byte & 0x0F];
	hexStr[2] = '\0';
}

static inline void DWordToHexStr(uint32_t dword, char hexStr[]) {
	ByteToHexStr(dword>>24, hexStr);
	ByteToHexStr(dword>>16, hexStr + 2);
	ByteToHexStr(dword>>8, hexStr + 4);
	ByteToHexStr(dword, hexStr + 6);
}

static inline void CheckUartIsr(uint8_t val) {
	uint8_t c;
	char hexByte[3];

	if (val != (c = UART_ISR)) {
		dtext("WARNING: ISR = 0x", 1);
		ByteToHexStr(c, hexByte);
		dtext(hexByte, 18);
	}
}

static inline void SpinTick(void) {
	static uint8_t beat = 0;
	static char spin[] = {'|', '\0'};

	if (!(beat++ & 0x3F)) {
		dtext(spin, 1);
		spin[0] = spinner[(beat>>6) & 3];
	}
}

// Function for testing UART TX
void UartTxLoop(void) {
	uint8_t c;
	char hex[3];
	// Test: write to scratchpad register and read the value back.
	UART_SPR = 0x55;
	c = UART_SPR;
	if (0x55 == c) {
		dtext("0x55 TEST SUCCEEDED!", 1);
	} else {
		dtext("0x55 TEST FAILED!:", 1);
		ByteToHexStr(c, hex);
		dtext(hex, 20);
	}
	UART_SPR = 0xAA;
	c = UART_SPR;
	if (0xAA == c) {
		dtext("0xAA TEST SUCCEEDED!", 1);
	} else {
		dtext("0xAA TEST FAILED!:", 1);
		ByteToHexStr(c, hex);
		dtext(hex, 20);
	}


	CheckUartIsr(0xC1);
	dtext("TRANSMIT START!", 1);

	while(1) {
		// UART transmission test
		SpinTick();
		while (!UartTxReady());
		// Fill FIFO (write 16 characters).
		for (c = '0'; c < '@'; c++) UartPutc(c);
		// Wait for VBLANK interval
		//VDP_waitVSync();
	}
}

// Uses loopback to echo characters
void UartLoopbackLoop(void) {
	uint8_t sent;
	volatile uint32_t i;

	for (i = 0; i < 100000; i++);
	MwModuleStart();
	for (i = 0; i < 100000; i++);

	CheckUartIsr(0xC1);

	UartResetFifos();
	dtext("LOOPBACK TEST START!", 1);
	sent = '0';
	UartPutc(sent);	// Tx first byte, that will be continuously echoed
	while (1) {
		if (UartRxReady()) {
			if (UartGetc() != sent) break;
			sent = '~' == sent?'0':sent + 1;
			UartPutc(sent);
			SpinTick();
		}
	}
	// Exit with error!
	dtext("LOOPBACK TEST ERROR!", 1);
}

// Echoes characters received
void UartEchoLoop(void) {
	uint8_t lsr;

	CheckUartIsr(0xC1);

	dtext("ECHO TEST START!", 1);

	while (1) {

		if ((lsr = UART_LSR) & 1) {
			UartPutc(UartGetc());
			SpinTick();
		}
		else if (lsr & 0x97) UartPutc('E');
	}
}

void MwModuleRun() {
	// Hold reset some time and then release it.
	dtext("Resetting WiFi...", 1);
	DelayFrames(30);
	MwModuleStart();
}

void MwEchoTest(void) {
	int i;

	// Try sending and receiving echo
	cmd.cmd = MW_CMD_ECHO;
	cmd.datalen = sizeof(echoTestStr) - 1;
	strcpy((char*)cmd.data, echoTestStr);
	dtext("Sending echo string...\n", 1);
	UartResetFifos();
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Echo recv failed!\n", 1);
		return;
	}
	dtext("Got response!\n", 1);
	for (i = 0; i < cmd.datalen; i++) {
		if (cmd.data[i] != rep.data[i]) break;
	}
	if (i != cmd.datalen) {
		dtext("Echo reply differs!", 1);
	}
	else {
		dtext("Echo test OK", 1);
	}
}

void MwHelloServTest(void) {
	const char helloStr[] = "Hello world, this is a MEGADRIVE!\n";
	char echoBuff[80];
	uint16_t len = 80 - 1;
	int done;

	// Create server socket and bind to port 7
	cmd.cmd = MW_CMD_TCP_BIND;
	cmd.datalen = 7;
	cmd.bind.reserved = 0;
	cmd.bind.port = 7;
	cmd.bind.channel = TCP_TEST_CH;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Bind failed!", 1);
		return;
	}
	dtext("Bound server socket to port 7.", 1);
	// Wait until a connection with a client is established
	done = FALSE;
	while (!done) {
		DelayFrames(60);	// Wait one second (or more on PAL systems)
		// Request system status
		cmd.cmd = MW_CMD_SYS_STAT;
		cmd.datalen = 0;
		MwCmdSend(&cmd);
		if (MwCmdReplyGet(&rep) < 0) {
			dtext("System status request failed!", 1);
			return;
		}
		// Check if there is an event available on socket
		if (cmd.sysStat.ch_ev) {
			// Check if event is on channel 1
			if (cmd.sysStat.ch_ev & (1<<TCP_TEST_CH)) {
				// Get socket status
				cmd.cmd = MW_CMD_SOCK_STAT;
				cmd.datalen = 1;
				cmd.data[0] = TCP_TEST_CH;
				MwCmdSend(&cmd);
				if (MwCmdReplyGet(&rep) < 0) {
					dtext("Couldn't get socket status!", 1);
					return;
				}
				switch (rep.data[0]) {
					case MW_SOCK_TCP_LISTEN:
						// No connection yet :-/
						break;
					
					case MW_SOCK_TCP_EST:
						// Connection established :-)
						// // Connection established :-)
						done = TRUE;
						break;
					
					default:
						dtext("Unexpected server socket status!", 1);
						return;
				}
				if (rep.data[0] == MW_SOCK_TCP_EST) done = TRUE;
			} else {
				dtext("Channel event on unexpected channel!", 1);
				return;
			}
		}
	} // while (!done)
	dtext("Connection established.", 1);
	// Enable channel
	LsdChEnable(TCP_TEST_CH);
	// Send hello string on channel 1
	LsdSend((uint8_t*)helloStr, sizeof(helloStr) - 1, TCP_TEST_CH);
	// Try receiving the echoed string
	if (TCP_TEST_CH == LsdRecv((uint8_t*)echoBuff, &len, UINT32_MAX)) {
		echoBuff[len] = '\0';
		VDP_drawText("Rx:", 1, line);
		dtext(echoBuff, 5);
	} else {
		dtext("Error waiting for data", 1);
	}
	// Disconnect from host
	cmd.cmd = MW_CMD_TCP_DISC;
	cmd.datalen = 1;
	cmd.data[0] = TCP_TEST_CH;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Disconnect failed!", 1);
	} else {
		dtext("Disconnected from host.", 1);
	}
}

void MwTcpHelloTest(void) {
	MwMsgInAddr* addr = (MwMsgInAddr*)cmd.data;
	const char dstport[] = "1234";
	const char dstaddr[] = "192.168.1.10";
	const char helloStr[] = "Hello world, this is a MEGADRIVE!\n";
	char echoBuff[80];
	uint16_t len = 80 - 1;

	// Configure TCP socket
	cmd.cmd = MW_CMD_TCP_CON;
	// Length is the length of both ports, the channel and the address.
	cmd.datalen = 6 + 6 + 1 + sizeof(dstaddr);
	memset(addr->dst_port, 0, cmd.datalen);
	strcpy(addr->dst_port, dstport);
	strcpy(addr->data, dstaddr);
	addr->channel = TCP_TEST_CH;

	// Try to establish connection
	dtext("Connecting to host...", 1);
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Connection failed!", 1);
		return;
	}
	// TODO check returned code
	dtext("Connecton established", 1);

	// Enable channel
	LsdChEnable(TCP_TEST_CH);
	// Send hello string on channel 1
	LsdSend((uint8_t*)helloStr, sizeof(helloStr) - 1, TCP_TEST_CH);
	// Try receiving the echoed string
	if (TCP_TEST_CH == LsdRecv((uint8_t*)echoBuff, &len, UINT32_MAX)) {
		echoBuff[len] = '\0';
		VDP_drawText("Rx:", 1, line);
		dtext(echoBuff, 5);
	} else {
		dtext("Error waiting for data", 1);
	}
	// Disconnect from host
	cmd.cmd = MW_CMD_TCP_DISC;
	cmd.datalen = 1;
	cmd.data[0] = TCP_TEST_CH;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Disconnect failed!", 1);
	} else {
		dtext("Disconnected from host.", 1);
	}
}

#define AUTH_MAX 5
void MwApScanPrint(MwCmd *rep) {
	// Character strings related to supported authentication modes
	const char *authStr[AUTH_MAX + 1] = {
		"OPEN", "WEP", "WPA_PSK", "WPA2_PSK", "WPA_WPA2_PSK", "???"
	};
	const uint8_t authStrLen[] = {
		4, 3, 7, 8, 12, 3
	};
	uint16_t i,  x;
	char hex[3];
	char ssid[33];

	i = 0;
	while (i < rep->datalen) {
		x = 1;
		// Print auth mode
		VDP_drawText(authStr[MIN(rep->data[i], AUTH_MAX)], x, line);
		x += authStrLen[MIN(rep->data[i], AUTH_MAX)];
		i++;
		VDP_drawText(", ", x, line);
		x += 2;
		// Print channel
		ByteToHexStr(rep->data[i++], hex);
		VDP_drawText(hex, x, line);
		x += 2;
		VDP_drawText(", ", x, line);
		x += 2;
		// Print strength
		ByteToHexStr(rep->data[i++], hex);
		VDP_drawText(hex, x, line);
		x += 2;
		VDP_drawText(", ", x, line);
		x += 2;
		// Print SSID
		memcpy(ssid, rep->data + i + 1, rep->data[i]);
		ssid[rep->data[i]] = '\0';
		VDP_drawText(ssid, x, line++);
		i += rep->data[i] + 1;
	}
}

void MwApConfig(void) {
	cmd.cmd = MW_CMD_AP_CFG;
	cmd.datalen = sizeof(MwMsgApCfg);
	cmd.apCfg.cfgNum = 0;
	strcpy(cmd.apCfg.ssid, WIFI_SSID);
	strcpy(cmd.apCfg.pass, WIFI_PASS);
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("AP configuration failed!", 1);
		return;
	}
	dtext("AP configuration OK!", 1);
}

void MwConfigGet(uint8_t num) {
	char hex[9];

	cmd.cmd = MW_CMD_AP_CFG_GET;
	cmd.datalen = 1;
	cmd.data[0] = num;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("AP CFG GET failed!", 1);
		return;
	}
	VDP_drawText("CFG ", 1, line);
	ByteToHexStr(num, hex);
	dtext(hex, 5);
	VDP_drawText("SSID: ", 1, line);
	dtext(rep.apCfg.ssid, 7);
	VDP_drawText("PASS: ", 1, line);
	dtext(rep.apCfg.pass, 7);
	
	cmd.cmd = MW_CMD_IP_CFG_GET;
	cmd.datalen = 1;
	cmd.data[0] = num;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("IP CFG GET failed!", 1);
		return;
	}
	VDP_drawText("IP:   ", 1, line);
	DWordToHexStr(rep.ipCfg.ip_addr, hex);
	dtext(hex, 7);
	VDP_drawText("MASK: ", 1, line);
	DWordToHexStr(rep.ipCfg.mask, hex);
	dtext(hex, 7);
	VDP_drawText("GW:   ", 1, line);
	DWordToHexStr(rep.ipCfg.gateway, hex);
	dtext(hex, 7);
	VDP_drawText("DNS1: ", 1, line);
	DWordToHexStr(rep.ipCfg.dns1, hex);
	dtext(hex, 7);
	VDP_drawText("DNS2: ", 1, line);
	DWordToHexStr(rep.ipCfg.dns2, hex);
	dtext(hex, 7);
}

void MwIpConfig(void) {
	cmd.cmd = MW_CMD_IP_CFG;
	cmd.datalen = sizeof(MwMsgIpCfg);
	cmd.ipCfg.cfgNum = 0;
	cmd.ipCfg.reserved[0] = 0;
	cmd.ipCfg.reserved[1] = 0;
	cmd.ipCfg.reserved[2] = 0;
	cmd.ipCfg.ip_addr = IPV4_BUILD(192, 168, 1, 60);
	cmd.ipCfg.mask    = IPV4_BUILD(255, 255, 255, 0);
	cmd.ipCfg.gateway = IPV4_BUILD(192, 168, 1, 5);
	cmd.ipCfg.dns1 = IPV4_BUILD(87, 216, 1, 65);
	cmd.ipCfg.dns2 = IPV4_BUILD(87, 216, 1, 66);
	MwCmdSend(&cmd);
	if ((MwCmdReplyGet(&rep) < 0) || (MW_CMD_OK != rep.cmd)) {
		dtext("IP configuration failed!", 1);
		return;
	}
	dtext("Configured static IP", 1);
}

void MwScanTest(void) {
	// Leave current AP
	dtext("Leaving AP...", 1);
	cmd.cmd = MW_CMD_AP_LEAVE;
	cmd.datalen = 0;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("AP leave failed!", 1);
		return;
	}
	// Start scan and get scan result
	dtext("AP left, starting scan", 1);
	cmd.cmd = MW_CMD_AP_SCAN;
	cmd.datalen = 0;
	MwCmdSend(&cmd);
	if ((MwCmdReplyGet(&rep) < 0) || (MW_CMD_OK != rep.cmd)) {
		dtext("AP Scan failed!", 1);
		return;
	}
	MwApScanPrint(&rep);
}

void MwSntpCfgSet(void) {
	uint8_t offset;
	const char *servers[3] = {"0.es.pool.ntp.org", "1.europe.pool.ntp.org",
	"3.europe.pool.ntp.org"};

	cmd.cmd = MW_CMD_SNTP_CFG;
	cmd.sntpCfg.upDelay = 60;
	cmd.sntpCfg.tz = 1;
	cmd.sntpCfg.dst = 1;
	strcpy(cmd.sntpCfg.servers, servers[0]);
	// Offset: server length + 1 ('\0')
	offset  = strlen(servers[0]) + 1;
	strcpy(cmd.sntpCfg.servers + offset, servers[1]);
	offset += strlen(servers[1]) + 1;
	strcpy(cmd.sntpCfg.servers + offset, servers[2]);
	offset += strlen(servers[2]) + 1;
	// Mark the end of the list with two adjacent '\0'
	cmd.sntpCfg.servers[offset] = '\0';
	cmd.datalen = offset + 1;
	MwCmdSend(&cmd);
	if ((MwCmdReplyGet(&rep) < 0) || (MW_CMD_OK != rep.cmd)) {
		dtext("SNTP configuration failed!", 1);
		return;
	}
	dtext("SNTP configuration set!", 1);
}

// Get date and time
void MwDatetimeGet(void) {
	char datetime[80]; 
	cmd.cmd = MW_CMD_DATETIME;
	cmd.datalen = 0;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Date and time query failed!", 1);
		return;
	}
	memcpy(datetime, rep.datetime.dtStr, rep.datalen - 2*sizeof(uint32_t));
	datetime[rep.datalen - 2*sizeof(uint32_t)] = '\0';
	dtext(datetime, 1);
}

// Query and print MegaWiFi version
void MwVersionGet(void) {
	char hex[3];
	char variant[80];

	cmd.cmd = MW_CMD_VERSION;
	cmd.datalen = 0;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Version query failed!", 1);
		return;
	}
	VDP_drawText("MegaWiFi cart version ", 1, line);
	ByteToHexStr(rep.data[0], hex);
	VDP_drawText(hex, 23, line);
	VDP_drawText(".", 25, line);
	ByteToHexStr(rep.data[1], hex);
	VDP_drawText(hex, 26, line);
	VDP_drawText("-", 28, line);
	memcpy(variant, rep.data + 2, rep.datalen - 2);
	variant[rep.datalen - 2] = '\0';
	VDP_drawText(variant, 29, line);
	VDP_drawText(" detected!", 29 + rep.datalen - 2, line++);
}

void MwFlashTest(void) {
	char hex[3];
	const char str[] = "MegaWiFi flash API test string!";

	// Obtain and print Flash manufacturer and device IDs
	cmd.cmd = MW_CMD_FLASH_ID;
	cmd.datalen = 0;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("FlashID query failed!", 1);
		return;
	}
	VDP_drawText("FlashIDs: ", 1, line);
	ByteToHexStr(rep.data[0], hex);
	VDP_drawText(hex, 11, line);
	ByteToHexStr(rep.data[1], hex);
	VDP_drawText(hex, 14, line);
	ByteToHexStr(rep.data[2], hex);
	VDP_drawText(hex, 17, line++);

	// Try reading some data
	cmd.cmd = MW_CMD_FLASH_READ;
	cmd.flRange.addr = 0;	// Corresponds to 0x80000
	cmd.flRange.len = 80;
	cmd.datalen = sizeof(MwMsgFlashRange);
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Flash read failed!\n", 1);
		return;
	}
	dtext("Flash read OK:", 1);
	dtext((const char*)rep.data, 1);

	// Erase sector
	cmd.cmd = MW_CMD_FLASH_ERASE;
	cmd.datalen = sizeof(uint16_t);
	cmd.flSect = 0;	// Corresponds to sector 0x80
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Sector erase failed!", 1);
		return;
	}
	dtext("Sector erase OK!", 1);

	// Write some data
	cmd.cmd = MW_CMD_FLASH_WRITE;
	cmd.datalen = sizeof(str) + sizeof(uint32_t);
	cmd.flData.addr = 0;
	strcpy((char*)cmd.flData.data, str);
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Flash write failed!", 1);
		return;
	}
	dtext("Flash write OK!", 1);
}

void MwApJoin(uint8_t num) {
	cmd.cmd = MW_CMD_AP_JOIN;
	cmd.datalen = 1;
	cmd.data[0] = num;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("AP join failed!", 1);
		return;
	}
	dtext("Joining AP...", 1);
}

// Get a bunch of numbers from the hardware random number generator
void MwHrngGet(void) {
	char hex[9];
	uint8_t i;

	cmd.cmd = MW_CMD_HRNG_GET;
	cmd.datalen = 2;
	cmd.rndLen = 4 * 4 * 16;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("HRNG get failed!", 1);
		return;
	}
	dtext("Dice roll:", 1);
	for (i = 0; i < 16; i++) {
		DWordToHexStr(rep.dwData[4 * i], hex);
		VDP_drawText(hex, 1, line);
		DWordToHexStr(rep.dwData[4 * i + 1], hex);
		VDP_drawText(hex, 10, line);
		DWordToHexStr(rep.dwData[4 * i + 2], hex);
		VDP_drawText(hex, 19, line);
		DWordToHexStr(rep.dwData[4 * i + 3], hex);
		dtext(hex, 28);
	}
}

void MwCfgDefaultSet(void) {
	cmd.cmd = MW_CMD_DEF_CFG_SET;
	cmd.datalen = 4;
	cmd.dwData[0] = 0xFEAA5501;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Factory reset failed!", 1);
		return;
	}
	dtext("Configuration reset to default.", 1);
}

int main(void) {
	line = 0;
	dtext("MeGaWiFi TEST PROGRAM", 1);

	// MegaWifi module initialization
	MwInit();

	//UartTxLoop();
	//UartEchoLoop();
	//UartSetBits(MCR, 0x10);	// Loopback enable
	//UartLoopbackLoop();

	MwModuleRun();
	// Wait 3 seconds for the module to start
	DelayFrames(3 * 60);
//	MwVersionGet();
	// Wait 6 additional seconds for the module to get ready
	dtext("Connecting to router...", 1);
	DelayFrames(6 * 60);
//	MwEchoTest();
//	MwTcpHelloTest();
	MwDatetimeGet();
	MwHelloServTest();
	MwHrngGet();
//	MwCfgDefaultSet();
//	MwConfigGet(0);
//	MwConfigGet(1);
//	MwConfigGet(2);
//	MwSntpCfgSet();
//	MwScanTest();
//	MwApJoin(1);
//	MwApConfig();
//	MwIpConfig();
//	MwFlashTest();

	while(1);

	return 0;	
}

