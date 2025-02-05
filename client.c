//------------------------------------------------------------------------------
/**
 * @file client.c
 * @author charles-park (charles.park@hardkernel.com)
 * @brief ODROID-C4 JIG Client App.
 * @version 2.0
 * @date 2024-11-25
 *
 * @package apt install iperf3, nmap, ethtool, usbutils, alsa-utils
 *
 * @copyright Copyright (c) 2022
 *
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <getopt.h>
#include <pthread.h>

//------------------------------------------------------------------------------
#include "lib_fbui/lib_fb.h"
#include "lib_fbui/lib_ui.h"
#include "lib_dev_check/lib_dev_check.h"
#include "protocol.h"

//------------------------------------------------------------------------------
//
// JIG Protocol(V2.0)
// https://docs.google.com/spreadsheets/d/1Of7im-2I5m_M-YKswsubrzQAXEGy-japYeH8h_754WA/edit#gid=0
//
//------------------------------------------------------------------------------
//#define __JIG_SELF_MODE__

#define CLIENT_FB       "/dev/fb0"
#define CLIENT_UART     "/dev/ttyS0"

// #define CLIENT_UI       "ui_c4_client.cfg"
#define CLIENT_UI       "ui_client.cfg"

#define UART_BAUDRATE   115200

/* NLP Printer Info */
#define NLP_MAX_CHAR    19
#define NLP_ERR_LINE    20

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define UID_ALIVE       0
#define UID_IPADDR      4
#define UID_STATUS      47

#define RUN_BOX_ON      RGB_TO_UINT(204, 204, 0)
#define RUN_BOX_OFF     RGB_TO_UINT(153, 153, 0)

//------------------------------------------------------------------------------
#define MAIN_LOOP_DELAY     500

#define FUNC_LOOP_DELAY     (100*1000)

#define CHECK_CMD_DELAY     (500*1000)
#define UPDATE_UI_DELAY     (500*1000)

// system state
enum { eSTATUS_WAIT, eSTATUS_RUN, eSTATUS_PRINT, eSTATUS_STOP, eSTATUS_END };

typedef struct client__t {
    // HDMI UI
    fb_info_t   *pfb;
    ui_grp_t    *pui;

    // UART communication
    uart_t      *puart;
    char        rx_msg [SERIAL_RESP_SIZE +1];
    char        tx_msg [SERIAL_RESP_SIZE +1];
}   client_t;


//------------------------------------------------------------------------------
#define DEFAULT_RUNING_TIME  30

volatile int SystemCheckReady = 0, RunningTime = DEFAULT_RUNING_TIME;
volatile int UIStatus = eSTATUS_WAIT;

pthread_t thread_ui;
pthread_t thread_check;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 문자열 변경 함수. 입력 포인터는 반드시 메모리가 할당되어진 변수여야 함.
//------------------------------------------------------------------------------
static void tolowerstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = tolower(*p);
}

//------------------------------------------------------------------------------
static void toupperstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = toupper(*p);
}

//------------------------------------------------------------------------------
int print_test_result (client_t *p)
{
    int check_item;
    int error_cnt, err_line = 0, pos = 0;
    char error_str [NLP_ERR_LINE][NLP_MAX_CHAR];
    char serial_resp[SERIAL_RESP_SIZE], resp[DEVICE_RESP_SIZE];

    // mac print
    memset  (resp, 0, sizeof(resp));
    sprintf (resp, "%c,%20s", 'P', get_mac_addr());
    SERIAL_RESP_FORM(serial_resp, 'M', 0, 0, resp);
    protocol_msg_tx (p->puart, serial_resp);    protocol_msg_tx (p->puart, "\r\n");

    // error count
    memset (error_str, 0, sizeof(error_str));
    for (check_item = 0, error_cnt = 0; check_item < p->pui->i_item_cnt; check_item++) {
        i_item_t *i_item = &p->pui->i_item[check_item];
        if ((i_item->status != 1) || (!i_item->complete)) {
            if (pos + strlen(i_item->name)+1 >= NLP_MAX_CHAR) {
                pos = 0, err_line++;
            }
            pos += sprintf (&error_str[err_line][pos],"%s ", i_item->name);
            error_cnt++;
        }
    }

    // error msg, device check end
    if (error_cnt) {
        for (pos = 0; pos < err_line +1; pos++) {
            memset  (resp, 0, sizeof(resp));
            sprintf (resp, "%d,%20s", pos, &error_str[pos][0]);
            SERIAL_RESP_FORM(serial_resp, 'E', 0, 0, resp);
            protocol_msg_tx (p->puart, serial_resp);    protocol_msg_tx (p->puart, "\r\n");
        }
    }

    memset  (resp, 0, sizeof(resp));
    sprintf (resp, "%c,%20s", error_cnt ? 'F' : 'P',
                                error_cnt ? "FAIL" : "PASS");
    SERIAL_RESP_FORM(serial_resp, 'X', 0, 0, resp);
    protocol_msg_tx (p->puart, serial_resp);    protocol_msg_tx (p->puart, "\r\n");

    return error_cnt;
}

//------------------------------------------------------------------------------
static void *thread_ui_func (void *pclient)
{
    static int onoff = 0;
    client_t *p = (client_t *)pclient;
    char ip_addr[20];

    memset (ip_addr,    0, sizeof(ip_addr));
    sprintf(ip_addr, "%s", get_board_ip());

    while (1) {
        onoff = !onoff;
        ui_set_ritem (p->pfb, p->pui, UID_ALIVE,
                    onoff ? COLOR_GREEN : p->pui->bc.uint, -1);
        ui_set_sitem (p->pfb, p->pui, UID_IPADDR, -1, -1, ip_addr);

        switch (UIStatus) {
            case eSTATUS_WAIT:
                if (SystemCheckReady)    UIStatus = eSTATUS_RUN;
                ui_set_sitem (p->pfb, p->pui, UID_STATUS, -1, -1, "WAIT");
                ui_set_ritem (p->pfb, p->pui, UID_STATUS, p->pui->bc.uint, -1);
                break;
            case eSTATUS_RUN:
                if (RunningTime) {
                    char run_str[16];

                    memset  (run_str, 0, sizeof(run_str));
                    sprintf (run_str, "Running(%d)", onoff ? RunningTime : RunningTime--);
                    ui_set_ritem (p->pfb, p->pui, UID_STATUS, onoff ? RUN_BOX_ON : RUN_BOX_OFF, -1);
                    ui_set_sitem (p->pfb, p->pui, UID_STATUS, -1, -1, run_str);
                } else UIStatus = eSTATUS_PRINT;

                break;
            case eSTATUS_PRINT:
                ui_set_sitem (p->pfb, p->pui, UID_STATUS, -1, -1, "STOP");
                ui_set_ritem (p->pfb, p->pui, UID_STATUS,
                    print_test_result (p) ? COLOR_RED : COLOR_GREEN, -1);
                UIStatus = eSTATUS_STOP;
                break;
            case eSTATUS_STOP:
                break;
            default :
                UIStatus = eSTATUS_WAIT;
                break;
        }
        if (onoff)  ui_update (p->pfb, p->pui, -1);

        usleep (UPDATE_UI_DELAY);
    }
    return pclient;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int find_item_uid (client_t *p, int gid, int did)
{
    int i;
    for (i = 0; i < p->pui->i_item_cnt; i++) {
        i_item_t *i_item = &p->pui->i_item[i];
        if ((i_item->grp_id == gid) && (i_item->dev_id == did))
            return i_item->ui_id;
    }
    printf ("%s : Cannot find uid. gid = %d, did = %d\n", __func__, gid, did);
    return -1;
}

//------------------------------------------------------------------------------
static int find_item_pos (client_t *p, int gid, int did)
{
    int i;
    for (i = 0; i < p->pui->i_item_cnt; i++) {
        i_item_t *i_item = &p->pui->i_item[i];
        if ((i_item->grp_id == gid) && (i_item->dev_id == did))
            return i;
    }
    printf ("%s : Cannot find i_item. gid = %d, did = %d\n", __func__, gid, did);
    return -1;
}

//------------------------------------------------------------------------------
static int update_ui_data (client_t *p, parse_resp_data_t *pdata)
{
    char pstr[DEVICE_RESP_SIZE];
    int uid = find_item_uid (p, pdata->gid, pdata->did);

    if (uid == -1)  return 0;

    if (pdata->status_c != 'C') {
        ui_set_ritem (p->pfb, p->pui, uid,
                (pdata->status_i == 1) ? COLOR_GREEN : COLOR_RED, -1);
    } else {
        /* C command received */
        if (device_resp_check(pdata)) {
            ui_set_ritem (p->pfb, p->pui, uid,
                    pdata->status_i ? COLOR_GREEN : COLOR_RED, -1);

            p->pui->i_item[find_item_pos (p, pdata->gid, pdata->did)].status = pdata->status_i;
            p->pui->i_item[find_item_pos (p, pdata->gid, pdata->did)].complete = 1;

            // 'C' command receive -> 'P' or 'F' command send to server
            {
                char serial_resp[SERIAL_RESP_SIZE], resp[DEVICE_RESP_SIZE];

                DEVICE_RESP_FORM_STR(resp, pdata->status_i ? 'P': 'F', pdata->resp_s);
                SERIAL_RESP_FORM(serial_resp, 'S', pdata->gid, pdata->did, resp);

                protocol_msg_tx (p->puart, serial_resp);    protocol_msg_tx (p->puart, "\r\n");
            }
        }
        else
            return 0;
    }

    memset (pstr, 0, sizeof(pstr));
        memcpy (pstr, pdata->resp_s, strlen(pdata->resp_s));

    switch (pdata->gid) {
        case eGID_SYSTEM:
            if (pdata->did == eSYSTEM_MEM) {
                memset  (pstr, 0, sizeof(pstr));
                sprintf (pstr, "%d GB", pdata->resp_i);
            }
            if (pdata->did == eSYSTEM_FB_SIZE) {
                memset  (pstr, 0, sizeof(pstr));
                sprintf (pstr, "%s", (pdata->status_i == 1) ? "PASS" : "FAIL");
            }
            break;
        case eGID_HEADER:
            memset  (pstr, 0, sizeof(pstr));
            sprintf (pstr, "%s", (pdata->status_i == 1) ? "PASS" : "FAIL");
            break;
        default :
            break;
    }
    ui_set_sitem (p->pfb, p->pui, uid, -1, -1, pstr);
    return 1;
}

//------------------------------------------------------------------------------
void client_data_check (client_t *p, int check_item, void *dev_resp)
{
    int gid = p->pui->i_item[check_item].grp_id;
    int did = p->pui->i_item[check_item].dev_id;

#if defined(__JIG_SELF_MODE__)
    // if self(server) mode
    {
        parse_resp_data_t pdata;
        if (device_resp_parse( dev_resp, &pdata)) {
            pdata.cmd = 'S'; pdata.gid = gid; pdata.did = did;
            update_ui_data (p, &pdata);
        }
    }
#endif
    // if client mode
    {
        char serial_resp[SERIAL_RESP_SIZE], *resp;
        SERIAL_RESP_FORM(serial_resp, 'S', gid, did, dev_resp);

        protocol_msg_tx (p->puart, serial_resp);
        protocol_msg_tx (p->puart, "\r\n");

        // cmd check 'C'
        resp = (char *)dev_resp;
        if (resp[0] == 'C')
            usleep (CHECK_CMD_DELAY);
    }
    usleep (FUNC_LOOP_DELAY);
}

//------------------------------------------------------------------------------
static void *thread_check_func (void *pclient)
{
    int check_item = 0, pass_item = 0, gid, did, uid;
    char dev_resp[DEVICE_RESP_SIZE];
    client_t *p = (client_t *)pclient;

    while (!SystemCheckReady)    usleep (FUNC_LOOP_DELAY);

    while (pass_item != p->pui->i_item_cnt) {
        for (check_item = 0, pass_item = 0; check_item < p->pui->i_item_cnt; check_item++) {

            uid = p->pui->i_item[check_item].ui_id;
            gid = p->pui->i_item[check_item].grp_id;
            did = p->pui->i_item[check_item].dev_id;

            switch (gid) {
                case eGID_LED:
                    if ((DEVICE_ID(did) == eLED_100M) || (DEVICE_ID(did) == eLED_1G)) {
                        // if iperf_value == 0 then skip eth led test
                        if (!get_ethernet_iperf())  {
                            printf ("%s : skip %d : %d, complete = %d\n",
                                __func__, gid, did, p->pui->i_item[check_item].complete);
                            continue;
                        }
                    }
                    break;
                default :
                    break;
            }
            memset (dev_resp, 0, sizeof(dev_resp));

            if (!p->pui->i_item[check_item].complete) {
                if (!p->pui->i_item[check_item].is_info)
                    ui_set_ritem (p->pfb, p->pui, uid, COLOR_YELLOW, -1);

                p->pui->i_item[check_item].status =
                            device_check (gid, did, dev_resp);

                printf ("\n%s : gid = %d, did = %d, complete = %d, status = %d, resp = %s\n",
                                __func__, gid, did,
                                p->pui->i_item[check_item].complete,
                                p->pui->i_item[check_item].status,
                                dev_resp);

#if defined (__JIG_SELF_MODE__)
                p->pui->i_item[check_item].complete =
                    (dev_resp[0] == 'C') ? 0 : p->pui->i_item[check_item].status;
#endif
                client_data_check (p, check_item, dev_resp);
            } else pass_item++;
        }
        // loop delay
        usleep (FUNC_LOOP_DELAY);
    }
    // check complete
    RunningTime = 0;    SystemCheckReady = 0;
    return pclient;
}

//------------------------------------------------------------------------------
static int client_setup (client_t *p)
{
    if ((p->pfb = fb_init (CLIENT_FB)) == NULL)         exit(1);
    if ((p->pui = ui_init (p->pfb, CLIENT_UI)) == NULL) exit(1);
    // ODROID-C4 (115200 baud)
    if ((p->puart = uart_init (CLIENT_UART, UART_BAUDRATE)) != NULL) {
        if (ptc_grp_init (p->puart, 1)) {
            if (!ptc_func_init (p->puart, 0, SERIAL_RESP_SIZE, protocol_check, protocol_catch)) {
                printf ("%s : protocol install error.", __func__);
                exit(1);
            }
        }
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
static void protocol_parse (client_t *p)
{
    parse_resp_data_t pitem;
    char *rx_msg = (char *)p->rx_msg;

    if (!device_resp_parse (rx_msg, &pitem))   return;

    switch (pitem.cmd) {
        case 'O':
            SystemCheckReady = 1;
            break;
        case 'X':
            // force stop
            if (!RunningTime)
                RunningTime = 1;
            break;
        case 'E':
            if (!RunningTime)
                print_test_result (p);
            break;
        case 'B':
            printf ("%s : server reboot!! client reboot!\n", __func__);
            fflush (stdout);
            exit (0);   // normal exit than app restart.
        case 'R':
            // item check status init (re-check)
            {
                int check_item = find_item_pos (p, pitem.gid, pitem.did);

                if (SystemCheckReady) {
                    p->pui->i_item[check_item].complete = 0;
                    p->pui->i_item[check_item].status = 0;
                    RunningTime += 5;
                } else {
                    char serial_resp[SERIAL_RESP_SIZE], dev_resp[DEVICE_RESP_SIZE];

                    memset (serial_resp, 0, sizeof(serial_resp));
                    memset (dev_resp, 0, sizeof(dev_resp));

                    p->pui->i_item[check_item].status =
                                device_check (pitem.gid, pitem.did, dev_resp);

                    SERIAL_RESP_FORM(serial_resp, 'S', pitem.gid, pitem.did, dev_resp);
                    protocol_msg_tx (p->puart, serial_resp);
                    protocol_msg_tx (p->puart, "\r\n");
                }
            }
            break;
        case 'C': case 'A':
            if (update_ui_data (p, &pitem)) {
                int check_item;
                if ((check_item = find_item_pos (p, pitem.gid, pitem.did)) != -1) {
                    p->pui->i_item[check_item].complete = 1;
                    printf ("%s : gid = %d, did = %d, ack received.\n",
                            __func__, pitem.gid, pitem.did);
                }
            }
            break;
        default :
            printf ("%s : unknown command!! (%c)\n", __func__, pitem.cmd);
            break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (void)
{
    client_t client;

    memset (&client, 0, sizeof(client));

    // UI, UART
    client_setup (&client);

    pthread_create (&thread_ui,    NULL, thread_ui_func,    (void *)&client);
    pthread_create (&thread_check, NULL, thread_check_func, (void *)&client);

    ui_set_popup (client.pfb, client.pui,
                    client.pfb->w * 80 / 100 , client.pfb->h * 30 / 100, 2,
                    COLOR_RED, COLOR_BLACK, COLOR_RED,
                    2, 1, "%s", "USB F/W Check & Upgrade");

    // client device init (lib_dev_check)
    device_setup ();

    // popup disable
    client.pui->p_item.timeout = 0;

    // Send boot msg & Wait for Ready msg
    {
        char serial_resp[SERIAL_RESP_SIZE];

        SystemCheckReady = 0;
        SERIAL_RESP_FORM(serial_resp, 'R', -1, -1, NULL);
        protocol_msg_tx (client.puart, serial_resp);    protocol_msg_tx (client.puart, "\r\n");
    }

#if defined (__JIG_SELF_MODE__)
    SystemCheckReady = 1;
#endif

    while (1) {
        if (protocol_msg_rx (client.puart, client.rx_msg))
            protocol_parse  (&client);
        usleep (MAIN_LOOP_DELAY);
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
