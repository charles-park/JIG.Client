//------------------------------------------------------------------------------
/**
 * @file protocol.h
 * @author charles-park (charles.park@hardkernel.com)
 * @brief ODROID-M1S JIG Client App.
 * @version 0.2
 * @date 2023-10-23
 *
 * @package apt install iperf3, nmap, ethtool, usbutils, alsa-utils
 *
 * @copyright Copyright (c) 2022
 *
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#ifndef	__PROTOCOL_H__
#define	__PROTOCOL_H__

#include "lib_uart/lib_uart.h"

//------------------------------------------------------------------------------
// function prototype define
//------------------------------------------------------------------------------
extern  int     protocol_catch  (ptc_var_t *var);
extern  int     protocol_check  (ptc_var_t *var);
extern  void    protocol_msg_tx (uart_t *puart, void *tx_msg);
extern  int     protocol_msg_rx (uart_t *puart, char *rx_msg);

//------------------------------------------------------------------------------
#endif	// #define	__PROTOCOL_H__

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------