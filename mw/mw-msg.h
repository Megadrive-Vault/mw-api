#ifndef _MW_MSG_H_
#define _MW_MSG_H_

#include <stdint.h>

/// Maximum buffer length (bytes)
#define MW_MSG_MAX_BUFLEN	512

#define MW_CMD_MAX_BUFLEN	(MW_MSG_MAX_BUFLEN - 4)

/// Maximum SSID length (including '\0').
#define MW_SSID_MAXLEN		32
/// Maximum password length (including '\0').
#define MW_PASS_MAXLEN		64

/** \addtogroup MwApi Cmds Supported commands.
 *  \{ */
#define MW_CMD_OK			  0		///< OK command reply
#define MW_CMD_VERSION        1		///< Get firmware version
#define MW_CMD_ECHO			  2		///< Echo data
#define MW_CMD_AP_SCAN		  3		///< Scan for access points
#define MW_CMD_AP_CFG		  4		///< Configure access point
#define MW_CMD_AP_CFG_GET     5		///< Get access point configuration
#define MW_CMD_IP_CFG		  6		///< Configure IPv4
#define MW_CMD_IP_CFG_GET	  7		///< Get IPv4 configuration
#define MW_CMD_AP_JOIN		  8		///< Join access point
#define MW_CMD_AP_LEAVE		  9		///< Leave previously joined access point
#define MW_CMD_TCP_CON		 10		///< Connect TCP socket
#define MW_CMD_TCP_BIND		 11		///< Bind TCP socket to port
#define MW_CMD_TCP_ACCEPT	 12		///< Accept incomint TCP connection
#define MW_CMD_TCP_DISC		 13		///< Disconnect and free TCP socket
#define MW_CMD_UDP_SET		 14		///< Configure UDP socket
#define MW_CMD_UDP_CLR		 15		///< Clear and free UDP socket
#define MW_CMD_SOCK_STAT	 16		///< Get socket status
#define MW_CMD_PING			 17		///< Ping host
#define MW_CMD_SNTP_CFG		 18		///< Configure SNTP service
#define MW_CMD_DATETIME		 19		///< Get date and time
#define MW_CMD_DT_SET        20		///< Set date and time
#define MW_CMD_FLASH_WRITE	 21		///< Write to WiFi module flash
#define MW_CMD_FLASH_READ	 22		///< Read from WiFi module flash
#define MW_CMD_FLASH_ERASE	 23		///< Erase sector from WiFi flash
#define MW_CMD_FLASH_ID 	 24		///< Get WiFi flash chip identifiers
#define MW_CMD_SYS_STAT		 25		///< Get system status
#define MW_CMD_DEF_CFG_SET	 26		///< Set default configuration
#define MW_CMD_HRNG_GET		 27		///< Gets random numbers
#define MW_CMD_ERROR		255		///< Error command reply
/** \} */

/// TCP/UDP address message
typedef struct {
	char dst_port[6];
	char src_port[6];
	uint8_t channel;
	char data[MW_CMD_MAX_BUFLEN - 6 - 6 - 1];
} MwMsgInAddr;

/// AP configuration message
typedef struct {
	uint8_t cfgNum;
	char ssid[MW_SSID_MAXLEN];
	char pass[MW_PASS_MAXLEN];
} MwMsgApCfg;

/// IP configuration message
typedef struct {
	uint8_t cfgNum;
	uint8_t reserved[3];
	uint32_t ip_addr;
	uint32_t mask;
	uint32_t gateway;
	uint32_t dns1;
	uint32_t dns2;
} MwMsgIpCfg;

/// SNTP and timezone configuration
typedef struct {
	uint16_t upDelay;
	int8_t tz;
	uint8_t dst;
	char servers[MW_CMD_MAX_BUFLEN - 4];
} MwMsgSntpCfg;

/// Date and time message
typedef struct {
	uint32_t dtBin[2];
	char dtStr[MW_CMD_MAX_BUFLEN - sizeof(uint64_t)];
} MwMsgDateTime;

typedef struct {
	uint32_t addr;
	uint8_t data[MW_CMD_MAX_BUFLEN - sizeof(uint32_t)];
} MwMsgFlashData;

typedef struct {
	uint32_t addr;
	uint16_t len;
} MwMsgFlashRange;

typedef struct {
	uint32_t reserved;
	uint16_t port;
	uint8_t  channel;
} MwMsgBind;

/** \addtogroup MwApi MwState Possible states of the system state machine.
 *  \{ */
typedef enum {
	MW_ST_INIT = 0,		///< Initialization state.
	MW_ST_IDLE,			///< Idle state, until connected to an AP.
	MW_ST_AP_JOIN,		///< Trying to join an access point.
	MW_ST_SCAN,			///< Scanning access points.
	MW_ST_READY,		///< Connected to The Internet.
	MW_ST_TRANSPARENT,	///< Transparent communication state.
	MW_ST_MAX			///< Limit number for state machine.
} MwState;
/** \} */

/** \addtogroup MwApi MwSockStat Socket status.
 *  \{ */
typedef enum {
	MW_SOCK_NONE = 0,	///< Unused socket.
	MW_SOCK_TCP_LISTEN,	///< Socket bound and listening.
	MW_SOCK_TCP_EST,	///< TCP socket, connection established.
	MW_SOCK_UDP_READY	///< UDP socket ready for sending/receiving
} MwSockStat;
/** \} */

/** \addtogroup MwApi MwSysStat System status
 *  \{ */
typedef union {
	uint32_t st_flags;
	struct {
		MwState sys_stat:8;		///< System status
		uint8_t online:1;		///< Module is connected to the Internet
		uint8_t cfg_ok:1;		///< Configuration OK
		uint8_t dt_ok:1;		///< Date and time synchronized at least once
		uint16_t reserved:5;	///< Reserved flags
		uint16_t ch_ev:16;		///< Channel flags with the pending event
	};
} MwMsgSysStat;
/** \} */

/** \addtogroup MwApi MwCmd Command sent to system FSM
 *  \{ */
typedef struct {
	uint16_t cmd;		///< Command code
	uint16_t datalen;	///< Data length
	// If datalen is nonzero, additional command data goes here until
	// filling datalen bytes.
	union {
		uint8_t ch;		// Channel number for channel related requests
		uint8_t data[MW_CMD_MAX_BUFLEN];// Might need adjusting data length!
		uint32_t dwData[MW_CMD_MAX_BUFLEN / sizeof(uint32_t)];
		MwMsgInAddr inAddr;
		MwMsgApCfg apCfg;
		MwMsgIpCfg ipCfg;
		MwMsgSntpCfg sntpCfg;
		MwMsgDateTime datetime;
		MwMsgFlashData flData;
		MwMsgFlashRange flRange;
		MwMsgBind bind;
		MwMsgSysStat sysStat;
		uint16_t flSect;	// Flash sector
		uint32_t flId;		// Flash IDs
		uint16_t rndLen;	// Length of the random buffer to fill
	};
} MwCmd;
/** \} */

typedef struct {
	uint8_t data[MW_MSG_MAX_BUFLEN];	///< Buffer data
	uint16_t len;						///< Length of buffer contents
	uint8_t ch;							///< Channel associated with buffer
} MwMsgBuf;

#endif //_MW_MSG_H_
