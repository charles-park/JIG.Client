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

    // UART communication
    uart_t      *puart;
    char        rx_msg [SERIAL_RESP_SIZE +1];
    char        tx_msg [SERIAL_RESP_SIZE +1];
}   client_t;

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
pthread_t thread_ui;

static void *thread_ui_func (void *pclient)
{
    static int onoff = 0;
    client_t *p = (client_t *)pclient;

    while (1) {
        ui_set_ritem (p->pfb, p->pui, ALIVE_DISPLAY_UI_ID,
                    onoff ? COLOR_GREEN : p->pui->bc.uint, -1);
        onoff = !onoff;

        if (onoff)  ui_update (p->pfb, p->pui, -1);

        usleep (APP_LOOP_DELAY * 1000);
    }
    return pclient;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#if 0
static int remove_space_str (char *str_value);
   /* 문자열이 없거나 앞부분의 공백이 있는 경우 제거 */
   if ((ptr = strtok (NULL, ",")) != NULL) {
      int slen = strlen(ptr);

      while ((*ptr == 0x20) && slen--)
         ptr++;
#endif
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
    char *ptr;

    if ((msg_size != SERIAL_RESP_SIZE) && (msg_size != DEVICE_RESP_SIZE)) {
        printf ("%s : unknown msg size = %d, msg = %s\n", __func__, msg_size, msg);
        return 0;
    }

    memset (parse_data, 0, sizeof(struct parse_item));

    if ((ptr = strtok (msg, ",")) != NULL) {
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
static int update_client_data (client_t *p, struct parse_item *pdata)
{
    char pstr[DEVICE_RESP_SIZE +1];
    int uid = find_uid (p, pdata->gid, pdata->did);

    if (uid == -1)  return 0;

    ui_set_ritem (p->pfb, p->pui, uid,
            (pdata->status_i == 1) ? COLOR_GREEN : COLOR_RED, -1);

    memset (pstr, 0, sizeof(pstr));
    memcpy (pstr, pdata->resp_s, strlen(pdata->resp_s));

    switch (pdata->gid) {
        case eGID_SYSTEM:
            if (pdata->did == eSYSTEM_MEM) {
                memset  (pstr, 0, sizeof(pstr));
                sprintf (pstr, "%d GB", pdata->resp_i);
            }
            break;
        case eGID_ETHERNET:
            if (pdata->did == eETHERNET_MAC) {
                memset  (pstr, 0, sizeof(pstr));
                sprintf (pstr, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
                    pdata->resp_s[0], pdata->resp_s[1], pdata->resp_s[2],  pdata->resp_s[3],
                    pdata->resp_s[4], pdata->resp_s[5], pdata->resp_s[6],  pdata->resp_s[7],
                    pdata->resp_s[8], pdata->resp_s[9], pdata->resp_s[10], pdata->resp_s[11]);
            }
            break;
        default :
            break;
    }
    ui_set_sitem (p->pfb, p->pui, uid, -1, -1, pstr);
    return 1;
}

//------------------------------------------------------------------------------
char *remove_space (char *pstr) {
    int slen = strlen(pstr);
   /* 문자열이 없거나 앞부분의 공백이 있는 경우 제거 */
    while ((*pstr == 0x20) && slen--)
        pstr++;

    return pstr;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
pthread_t thread_check;

static void *thread_check_func (void *pclient)
{
    static int RunningTime = 60, check_item = 0;
    int gid, did, uid;
    char dev_resp[DEVICE_RESP_SIZE];
    client_t *p = (client_t *)pclient;

    while (1) {
        for (check_item = 0; check_item < p->pui->i_item_cnt; check_item++) {
            uid = p->pui->i_item[check_item].ui_id;
            gid = p->pui->i_item[check_item].grp_id;
            did = p->pui->i_item[check_item].dev_id;

            memset (dev_resp, 0, sizeof(dev_resp));

            if (!p->pui->i_item[check_item].complete) {
                ui_set_ritem (p->pfb, p->pui, uid, COLOR_YELLOW, -1);

                p->pui->i_item[check_item].complete =
                        device_check (p->pui->i_item[check_item].grp_id,
                                    p->pui->i_item[check_item].dev_id, dev_resp);
            }

            printf ("\n%s : gid = %d, did = %d, complete = %d, resp = %s\n",
                    __func__, p->pui->i_item[check_item].grp_id,
                            p->pui->i_item[check_item].dev_id,
                            p->pui->i_item[check_item].complete, dev_resp);
            // if self(server) mode
            if (p->pui->i_item[check_item].complete) {
                struct parse_item pdata;
                if (parse_data( dev_resp, &pdata)) {
                    pdata.cmd = 'S';
                    pdata.gid = gid;
                    pdata.did = did;
                    update_client_data (p, &pdata);
                }
            }
            // if client mode
            // send tx
            // wait response
//            usleep (APP_LOOP_DELAY * 1000);
            usleep (100 * 1000);
        }
    }
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
#if 0
    int status = 0, int_ui_id = 0;
    char resp[SIZE_RESP_BYTES +1], str_ui_id[SIZE_UI_ID +1];

    // Server reboot cmd
    if (p->rx_msg[1] == 'P') {
        // Ready msg send
        protocol_msg_tx (p->puart, 'R', 0, "000000");
        return;
    }
    memset (str_ui_id, 0, SIZE_UI_ID);
    memcpy (str_ui_id, &p->rx_msg[2], SIZE_UI_ID);

    int_ui_id = atoi (str_ui_id);

    memset (resp, 0, SIZE_RESP_BYTES);
    status = device_check (p->rx_msg, resp);
    if (status < 0)
        protocol_msg_tx (p->puart, 'B', int_ui_id, resp);
    else
        protocol_msg_tx (p->puart, status ? 'O' : 'E', int_ui_id, resp);
#endif
}

//------------------------------------------------------------------------------
int main (void)
{
    client_t client;

    // UI, UART
    client_setup (&client);

    // client device init (lib_dev_check)
    device_setup ();

    pthread_create (&thread_ui,    NULL, thread_ui_func,    (void *)&client);
    pthread_create (&thread_check, NULL, thread_check_func, (void *)&client);
    // Send boot msg & Wait for Ready msg
//    protocol_msg_tx (client.puart, 'R', 0, "000000");
//  protocol_wait (client.puart, 'P');...
// protocol_msg_tx (client.puart, "123454566\r\n");

    while (1) {
        if (protocol_msg_rx (client.puart, client.rx_msg))
            protocol_parse  (&client);

        usleep (APP_LOOP_DELAY);
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
