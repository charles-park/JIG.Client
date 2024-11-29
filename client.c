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

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>

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
#define CLIENT_FB       "/dev/fb0"
#define CLIENT_UART     "/dev/ttyS0"

#define CLIENT_UI       "ui_c4_client.cfg"

#define BOARD_BAUDRATE          115200

#define ALIVE_DISPLAY_UI_ID     0
#define APP_LOOP_DELAY          500

typedef struct client__t {
    // HDMI UI
    fb_info_t   *pfb;
    ui_grp_t    *pui;

    char        mac[20];
    char        bip[20];
    // UART communication
    uart_t      *puart;
    char        rx_msg [SERIAL_RESP_SIZE +1];
    char        tx_msg [SERIAL_RESP_SIZE +1];
}   client_t;

//------------------------------------------------------------------------------
#define DEFAULT_CHECK_TIME  30

volatile int SystemCheckReady = 0, RunningTime = DEFAULT_CHECK_TIME;

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
static int run_interval_check (struct timeval *t, double interval_ms)
{
    struct timeval base_time;
    double difftime;

    gettimeofday(&base_time, NULL);

    if (interval_ms) {
        /* 현재 시간이 interval시간보다 크면 양수가 나옴 */
        difftime = (base_time.tv_sec - t->tv_sec) +
                    ((base_time.tv_usec - (t->tv_usec + interval_ms * 1000)) / 1000000);

        if (difftime > 0) {
            t->tv_sec  = base_time.tv_sec;
            t->tv_usec = base_time.tv_usec;
            return 1;
        }
        return 0;
    }
    /* 현재 시간 저장 */
    t->tv_sec  = base_time.tv_sec;
    t->tv_usec = base_time.tv_usec;
    return 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define NLP_MAX_CHAR        19
#define NLP_ERR_LINE        10

int print_test_result (client_t *p)
{
    int check_item, error_cnt, err_line = 0, pos = 0;
    char error_str [NLP_ERR_LINE][NLP_MAX_CHAR+1];
    // mac print
    {
        char serial_resp[SERIAL_RESP_SIZE], resp[DEVICE_RESP_SIZE];

        memset  (resp, 0, sizeof(resp));
        sprintf (resp, "%c,%20s", 'P', get_mac_addr());
        SERIAL_RESP_FORM(serial_resp, 'M', 0, 0, resp);
        protocol_msg_tx (p->puart, serial_resp);
        protocol_msg_tx (p->puart, "\r\n");
//        usleep (100*1000);
    }

    // error count
#if 0
    memset (error_str, 0, sizeof(error_str));
    for (check_item = 0, error_cnt = 0; check_item < p->pui->i_item_cnt; check_item++) {
        if (p->pui->i_item[check_item].status != 1) {
            if (pos + strlen(p->pui->i_item[check_item].name) > NLP_MAX_CHAR) {
printf ("%s : %s(%d)\n", __func__, error_str[err_line][0], pos);
                pos = 0, err_line++;
            }
            pos += sprintf (&error_str[err_line][pos],"%s ", p->pui->i_item[check_item].name);
            error_cnt++;
        }
    }
    if (error_cnt) {
        char serial_resp[SERIAL_RESP_SIZE], resp[DEVICE_RESP_SIZE];
        int i;

        for (i = 0; i < err_line; i++) {
            memset  (resp, 0, sizeof(resp));
            sprintf (resp, "%d,%20s", i, &error_str[i][0]);
            SERIAL_RESP_FORM(serial_resp, 'E', 0, 0, resp);
            protocol_msg_tx (p->puart, serial_resp);
            protocol_msg_tx (p->puart, "\r\n");
            usleep (100*1000);
        }
        #if 1
        memset  (resp, 0, sizeof(resp));
        sprintf (resp, "%c,%20s", error_cnt ? 'F' : 'P',
                                  error_cnt ? "FAIL" : "PASS");
        SERIAL_RESP_FORM(serial_resp, 'X', 0, 0, resp);
        protocol_msg_tx (p->puart, serial_resp);
        protocol_msg_tx (p->puart, "\r\n");
        usleep (100*1000);
        #endif
    }
#endif
    return error_cnt;
}

//------------------------------------------------------------------------------
#define UI_ID_IPADDR    4
pthread_t thread_ui;

enum { eSTATUS_WAIT, eSTATUS_RUN, eSTATUS_PRINT, eSTATUS_STOP, eSTATUS_END };

volatile int UIStatus = eSTATUS_WAIT;

static void *thread_ui_func (void *pclient)
{
    static int onoff = 0;
    client_t *p = (client_t *)pclient;

    ui_set_sitem (p->pfb, p->pui, UI_ID_IPADDR, -1, -1, get_board_ip());
    ui_update (p->pfb, p->pui, -1);

    while (1) {
        ui_set_ritem (p->pfb, p->pui, ALIVE_DISPLAY_UI_ID,
                    onoff ? COLOR_GREEN : p->pui->bc.uint, -1);
        onoff = !onoff;

        if (onoff)  {
            switch (UIStatus) {
                case eSTATUS_WAIT:
                    if (SystemCheckReady)    UIStatus = eSTATUS_RUN;
                    ui_set_sitem (p->pfb, p->pui, 47, -1, -1, "WAIT");
                    ui_set_ritem (p->pfb, p->pui, 47, p->pui->bc.uint, -1);
                    break;
                case eSTATUS_RUN:
                    if (RunningTime) {
                        char run_str[16];

                        memset  (run_str, 0, sizeof(run_str));
                        sprintf (run_str, "Running(%d)", RunningTime--);
//                        ui_set_ritem (p->pfb, p->pui, 47, COLOR_YELLOW, -1);
                        ui_set_ritem (p->pfb, p->pui, 47, COLOR_DARK_ORANGE, -1);
                        ui_set_sitem (p->pfb, p->pui, 47, -1, -1, run_str);
                    } else UIStatus = eSTATUS_PRINT;

                    break;
                case eSTATUS_PRINT:
                    ui_set_sitem (p->pfb, p->pui, 47, -1, -1, "STOP");
                    ui_set_ritem (p->pfb, p->pui, 47,
                        print_test_result (p) ? COLOR_RED : COLOR_GREEN, -1);
                    UIStatus = eSTATUS_STOP;
                    break;
                case eSTATUS_STOP:
                    break;
                default :
                    UIStatus = eSTATUS_WAIT;
                    break;
            }
            ui_update (p->pfb, p->pui, -1);
        }
        usleep (APP_LOOP_DELAY * 1000);
    }
    return pclient;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
struct parse_item {
    char    cmd;
    int     gid;
    int     did;
    char    status_c;
    int     status_i;
    char    resp_s[DEVICE_RESP_SIZE +1];
    int     resp_i;
};

static int parse_data (char *msg, struct parse_item *parse_data);
static int parse_data (char *msg, struct parse_item *parse_data)
{
    int msg_size = (int)strlen(msg);
    char *ptr, resp[DEVICE_RESP_SIZE+1];

    if ((msg_size != SERIAL_RESP_SIZE) && (msg_size != DEVICE_RESP_SIZE)) {
        printf ("%s : unknown msg size = %d, msg = %s\n", __func__, msg_size, msg);
        return 0;
    }

    memset (resp, 0, sizeof(resp));
    memcpy (resp, msg, DEVICE_RESP_SIZE);
    memset (parse_data, 0, sizeof(struct parse_item));

    if ((ptr = strtok (resp, ",")) != NULL) {
        if (msg_size == SERIAL_RESP_SIZE) {
            // cmd
            if ((ptr = strtok (NULL, ",")) != NULL)
                parse_data->cmd = *ptr;
            // gid
            if ((ptr = strtok (NULL, ",")) != NULL)
                parse_data->gid = atoi(ptr);
            // did
            if ((ptr = strtok (NULL, ",")) != NULL)
                parse_data->did = atoi(ptr);
        }
        // status
        if (ptr != NULL) {
            parse_data->status_c =  *ptr;
            parse_data->status_i = (*ptr == 'P') ? 1 : 0;
        }
        // resp str
        if ((ptr = strtok (NULL, ",")) != NULL) {
            {
                int i, pos;
                for (i = 0, pos = 0; i < DEVICE_RESP_SIZE; i++)
                {
                    if (*(ptr + i) != 0x20)
                        parse_data->resp_s[pos++] = *(ptr + i);
                }
            }
            parse_data->resp_i = atoi(ptr);
        }
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
static int find_uid (client_t *p, int gid, int did)
{
    int i;
    for (i = 0; i < p->pui->i_item_cnt; i++) {
        if ((p->pui->i_item[i].grp_id == gid) && (p->pui->i_item[i].dev_id == did))
            return p->pui->i_item[i].ui_id;
    }
    printf ("%s : Cannot find uid. gid = %d, did = %d\n", __func__, gid, did);
    return -1;
}

//------------------------------------------------------------------------------
static int find_i_item (client_t *p, int gid, int did)
{
    int i;
    for (i = 0; i < p->pui->i_item_cnt; i++) {
        if ((p->pui->i_item[i].grp_id == gid) && (p->pui->i_item[i].dev_id == did))
            return i;
    }
    printf ("%s : Cannot find i_item. gid = %d, did = %d\n", __func__, gid, did);
    return -1;
}

//------------------------------------------------------------------------------
int device_data_check (struct parse_item *pdata)
{
    switch (pdata->gid) {
        /* IR Thread running */
        case eGID_IR:
            return 0;
         case eGID_ETHERNET:
            if (pdata->did == eETHERNET_IPERF)  return 0;
            break;
        case eGID_LED:
            pdata->status_i = led_data_check (pdata->did, pdata->resp_i);
            break;
        case eGID_HEADER:
            pdata->status_i = header_data_check (pdata->did, pdata->resp_s);
            break;

        /* not implement */
        case eGID_PWM: case eGID_GPIO: case eGID_AUDIO:
        default:
            pdata->status_i = 0;
            break;
    }
    // device_data_check ok
    return 1;
}

//------------------------------------------------------------------------------
static int update_client_data (client_t *p, struct parse_item *pdata)
{
    char pstr[DEVICE_RESP_SIZE];
    int uid = find_uid (p, pdata->gid, pdata->did);

    if (uid == -1)  return 0;

    if (pdata->status_c != 'C') {
        ui_set_ritem (p->pfb, p->pui, uid,
                (pdata->status_i == 1) ? COLOR_GREEN : COLOR_RED, -1);
    } else {
        if (device_data_check(pdata)) {
            ui_set_ritem (p->pfb, p->pui, uid,
                    pdata->status_i ? COLOR_GREEN : COLOR_RED, -1);

            p->pui->i_item[find_i_item(p, pdata->gid, pdata->did)].status = pdata->status_i;
            p->pui->i_item[find_i_item(p, pdata->gid, pdata->did)].complete = 1;

            // 'C' command receive -> 'P' or 'F' command send to server
            {
                char serial_resp[SERIAL_RESP_SIZE], resp[DEVICE_RESP_SIZE];

                memset  (resp, 0, sizeof(resp));
                sprintf (resp, "%c,%20s", pdata->status_i ? 'P': 'F', pdata->resp_s);

                SERIAL_RESP_FORM(serial_resp, 'S', pdata->gid, pdata->did, resp);

                protocol_msg_tx (p->puart, serial_resp);
                protocol_msg_tx (p->puart, "\r\n");
            }
        }
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
        default :
            break;
    }
    ui_set_sitem (p->pfb, p->pui, uid, -1, -1, pstr);
    return 1;
}

//------------------------------------------------------------------------------
#define __JIG_SELF_MODE__

void client_data_check (client_t *p, int check_item, void *dev_resp)
{
    int gid = p->pui->i_item[check_item].grp_id;
    int did = p->pui->i_item[check_item].dev_id;

#if defined(__JIG_SELF_MODE__)
    // if self(server) mode
    {
        struct parse_item pdata;
        if (parse_data( dev_resp, &pdata)) {
            pdata.cmd = 'S';
            pdata.gid = gid;
            pdata.did = did;
            update_client_data (p, &pdata);
        }
    }
#endif
    // if client mode
    {
        char serial_resp[SERIAL_RESP_SIZE], *resp;
        SERIAL_RESP_FORM(serial_resp, 'S', gid, did, dev_resp);

        protocol_msg_tx (p->puart, serial_resp);
        protocol_msg_tx (p->puart, "\r\n");

        // ADC Check delay
        resp = (char *)dev_resp;
        if (resp[0] == 'C')
            usleep (APP_LOOP_DELAY * 1000);
    }
    usleep (100 * 1000);
}

//------------------------------------------------------------------------------
pthread_t thread_check;

static void *thread_check_func (void *pclient)
{
    int check_item = 0, pass_item = 0, gid, did, uid;
    char dev_resp[DEVICE_RESP_SIZE];
    client_t *p = (client_t *)pclient;

    while (!SystemCheckReady)    usleep (APP_LOOP_DELAY * 1000);

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
                            printf ("%s : skip %d : %d\n", __func__, gid, did);
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
        usleep (APP_LOOP_DELAY * 1000);
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
    if ((p->puart = uart_init (CLIENT_UART, BOARD_BAUDRATE)) != NULL) {
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
    struct parse_item pitem;
    char *rx_msg = (char *)p->rx_msg;

    if (!parse_data (rx_msg, &pitem))   return;

    switch (pitem.cmd) {
        case 'O':
            SystemCheckReady = 1;
            break;
        case 'B':
            printf ("%s : server reboot!! client reboot!\n", __func__);
            fflush (stdout);
            exit (0);   // normal exit than app restart.
        case 'C': case 'A':
            if (update_client_data (p, &pitem)) {
                int check_item;
                if ((check_item = find_i_item (p, pitem.gid, pitem.did)) != -1) {
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
                    client.pfb->w /2 , client.pfb->h /4, 2,
                    COLOR_RED, COLOR_BLACK, COLOR_RED,
                    2, 1, "%s", "USB F/W Check & Upgrade");

    // client device init (lib_dev_check)
    device_setup ();

    // popup disable
    client.pui->p_item.timeout = 0;

    // Send boot msg & Wait for Ready msg
    {
        char serial_resp[SERIAL_RESP_SIZE];
        SERIAL_RESP_FORM(serial_resp, 'R', -1, -1, NULL);
        protocol_msg_tx (client.puart, serial_resp);
        protocol_msg_tx (client.puart, "\r\n");
    }

#if defined (__JIG_SELF_MODE__)
    SystemCheckReady = 1;
#endif

    while (1) {
        if (protocol_msg_rx (client.puart, client.rx_msg))
            protocol_parse  (&client);
        usleep (APP_LOOP_DELAY);
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
