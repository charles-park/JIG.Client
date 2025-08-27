//------------------------------------------------------------------------------
/**
 * @file client.h
 * @author charles-park (charles.park@hardkernel.com)
 * @brief ODROID-JIG Client App.
 * @version 0.2
 * @date 2025-08-27
 *
 * @package apt install iperf3, nmap, ethtool, usbutils, alsa-utils
 *
 * @copyright Copyright (c) 2022
 *
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#ifndef	__CLIENT_H__
#define	__CLIENT_H__

//------------------------------------------------------------------------------
#include "lib_fbui/lib_fb.h"
#include "lib_fbui/lib_ui.h"
#include "lib_fbui/lib_ts.h"
#include "lib_uart/lib_uart.h"
#include "lib_dev_check/lib_dev_check.h"
#include "protocol.h"

//------------------------------------------------------------------------------
#define CLIENT_HW_CONFIG        "client.cfg"

#define DEFAULT_CLIENT_FB       "/dev/fb0"
#define DEFAULT_CLIENT_UART     "/dev/ttyS0"
#define DEFAULT_UART_BAUDRATE   115200
#define DEFAULT_RUNING_TIME     60

//------------------------------------------------------------------------------
/* NLP Printer Info */
//------------------------------------------------------------------------------
#define NLP_MAX_CHAR    19
#define NLP_ERR_LINE    20

//------------------------------------------------------------------------------
// UI Status BOX id
//------------------------------------------------------------------------------
#define UID_ALIVE           0
#define UID_IPADDR          4
#define UID_STATUS          47

#define RUN_BOX_ON          RGB_TO_UINT(204, 204, 0)
#define RUN_BOX_OFF         RGB_TO_UINT(153, 153, 0)

//------------------------------------------------------------------------------
#define MAIN_LOOP_DELAY     500

#define FUNC_LOOP_DELAY     (100*1000)

#define CHECK_CMD_DELAY     (500*1000)
#define UPDATE_UI_DELAY     (500*1000)

//------------------------------------------------------------------------------
// system state
//------------------------------------------------------------------------------
enum { eSTATUS_WAIT, eSTATUS_RUN, eSTATUS_PRINT, eSTATUS_STOP, eSTATUS_END };

//------------------------------------------------------------------------------
typedef struct client__t {
    // UI Test remain time
    int         remain_time;

    // HDMI UI
    fb_info_t   *pfb;
    ui_grp_t    *pui;

    // model name str
    char        model[STR_NAME_LENGTH];

    // UART dev
    char        uart_dev[STR_NAME_LENGTH];
    int         uart_baud;

    // UART communication
    uart_t      *puart;
    char        rx_msg [SERIAL_RESP_SIZE +1];
    char        tx_msg [SERIAL_RESP_SIZE +1];
}   client_t;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// function prototype define
//------------------------------------------------------------------------------
// setup.c
//------------------------------------------------------------------------------
extern  int client_setup (client_t *p);

//------------------------------------------------------------------------------
#endif	// #define	__CLIENT_H__

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------