/************************************************************************//**
 * \brief  Local Symmetric Data-link. Implements an extremely simple
 *         protocol to link two full-duplex devices, multiplexing the
 *         data link.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 * \todo   Implement UART RTS/CTS handshaking.
 * \todo   Current implementation uses polling. Unfortunately as the Genesis/
 *         Megadrive does not have an interrupt pin on the cart, implementing
 *         more efficient data transmission techniques will be tricky.
 * \todo   Proper implementation of error handling.
 * \defgroup lsd Local Symmetric Data-link
 * \{
 ****************************************************************************/

/**
 * USAGE:
 * First initialize the module calling LsdInit().
 * Then enable at least one channel calling LsdEnable().
 *
 * To send data call LsdSend();
 *
 * Frame format is:
 *
 * STX : CH-LENH : LENL : DATA : ETX
 *
 * - STX and ETX are the start/end of transmission characters (1 byte each).
 * - CH-LENH is the channel number (first 4 bits) and the 4 high bits of the
 *   data length.
 * - LENL is the low 8 bits of the data length.
 * - DATA is the payload, of the previously specified length.
 */
#ifndef _LSD_H_
#define _LSD_H_

#include "16c550.h"
#include "mw-msg.h"

/** \addtogroup lsd ReturnCodes OK/Error codes returned by several functions.
 *  \{ */
/// Function completed successfully
#define LSD_OK				0
/// Generic error code
#define LSD_ERROR			-1
/// A framing error occurred. Possible data loss.
#define LSD_FRAMING_ERROR	-2
/** \} */

/// LSD frame overhead in bytes
#define LSD_OVERHEAD		4

/// Uart used for LSD
#define LSD_UART			0

/// Start/end of transmission character
#define LSD_STX_ETX		0x7E

/// Maximum number of available simultaneous channels
#define LSD_MAX_CH			4

/// Receive task priority
#define LSD_RECV_PRIO		2

/// Maximum data payload length
#define LSD_MAX_LEN		 4095

/************************************************************************//**
 * Module initialization. Call this function before any other one in this
 * module.
 ****************************************************************************/
void LsdInit(void);

/************************************************************************//**
 * Enables a channel to start reception and be able to send data.
 *
 * \param[in] ch Channel number.
 *
 * \return A pointer to an empty TX buffer, or NULL if no buffer is
 *         available.
 ****************************************************************************/
int LsdChEnable(uint8_t ch);

/************************************************************************//**
 * Disables a channel to stop reception and prohibit sending data.
 *
 * \param[in] ch Channel number.
 *
 * \return A pointer to an empty TX buffer, or NULL if no buffer is
 *         available.
 ****************************************************************************/
int LsdChDisable(uint8_t ch);


/************************************************************************//**
 * Sends data through a previously enabled channel.
 *
 * \param[in] data Buffer to send.
 * \param[in] len  Length of the buffer to send.
 * \param[in] ch   Channel number to use.
 *
 * \return -1 if there was an error, or the number of characterse sent
 * 		   otherwise.
 ****************************************************************************/
int LsdSend(uint8_t *data, uint16_t len, uint8_t ch);

/************************************************************************//**
 * Starts sending data through a previously enabled channel. Once started,
 * you can send more additional data inside of the frame by issuing as
 * many LsdSplitNext() calls as needed, and end the frame by calling
 * LsdSplitEnd().
 *
 * \param[in] data  Buffer to send.
 * \param[in] len   Length of the data buffer to send.
 * \param[in] total Total length of the data to send using a split frame.
 * \param[in] ch    Channel number to use for sending.
 *
 * \return -1 if there was an error, or the number of characterse sent
 * 		   otherwise.
 ****************************************************************************/
int LsdSplitStart(uint8_t *data, uint16_t len,
		              uint16_t total, uint8_t ch);

/************************************************************************//**
 * Appends (sends) additional data to a frame previously started by an
 * LsdSplitStart() call.
 *
 * \param[in] data  Buffer to send.
 * \param[in] len   Length of the data buffer to send.
 *
 * \return -1 if there was an error, or the number of characterse sent
 * 		   otherwise.
 ****************************************************************************/
int LsdSplitNext(uint8_t *data, uint16_t len);

/************************************************************************//**
 * Appends (sends) additional data to a frame previously started by an
 * LsdSplitStart() call, and finally ends the frame.
 *
 * \param[in] data  Buffer to send.
 * \param[in] len   Length of the data buffer to send.
 *
 * \return -1 if there was an error, or the number of characterse sent
 * 		   otherwise.
 ****************************************************************************/
int LsdSplitEnd(uint8_t *data, uint16_t len);

int LsdRecv(uint8_t* buf, uint16_t* maxLen, uint32_t maxLoopCnt);
//int LsdRecv(MwMsgBuf* buf, uint16_t maxLen, uint32_t maxLoopCnt);

/** \} */

#endif //_LSD_H_

