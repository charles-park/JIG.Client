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
pthread_t thread_ui;

enum { eSTATUS_WAIT, eSTATUS_RUN, eSTATUS_PRINT, eSTATUS_STOP, eSTATUS_END };

volatile int UIStatus = eSTATUS_WAIT;

static void *thread_ui_func (void *pclient)
{
    static int onoff = 0;
    client_t *p = (client_t *)pclient;

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
                        ui_set_sitem (p->pfb, p->pui, 47, -1, -1, run_str);
                    } else UIStatus = eSTATUS_PRINT;

                    break;
                case eSTATUS_PRINT:
                    // print_macaddr();
                    // print_error();
                    UIStatus = eSTATUS_STOP;
                    ui_set_sitem (p->pfb, p->pui, 47, -1, -1, "STOP");
                    ui_set_ritem (p->pfb, p->pui, 47,
                        SystemCheckReady ? COLOR_RED : COLOR_GREEN, -1);
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
int client_self_check (struct parse_item *pdata)
{
    /* IR Thread running */
    if (pdata->gid == eGID_IR)  return 0;

    // not implement

    // self check ok
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
        int status = client_self_check(pdata);

        if (status) {
            ui_set_ritem (p->pfb, p->pui, uid,
                    pdata->status_i ? COLOR_GREEN : COLOR_RED, -1);

            p->pui->i_item[find_i_item(p, pdata->gid, pdata->did)].complete = 1;
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
//#define __JIG_SELF_MODE__

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
        char serial_resp[SERIAL_RESP_SIZE];
        SERIAL_RESP_FORM(serial_resp, 'S', gid, did, dev_resp);
        protocol_msg_tx (p->puart, serial_resp);
        protocol_msg_tx (p->puart, "\r\n");
    }
    usleep (100 * 1000);
}

//------------------------------------------------------------------------------
pthread_t thread_check;

static void *thread_check_func (void *pclient)
{
    int check_item = 0, pass_item = 0;;
    int gid, did, uid;
    char dev_resp[DEVICE_RESP_SIZE];
    client_t *p = (client_t *)pclient;

    while (!SystemCheckReady)    usleep (APP_LOOP_DELAY * 1000);

    while (pass_item != p->pui->i_item_cnt) {
        for (check_item = 0, pass_item = 0; check_item < p->pui->i_item_cnt; check_item++) {
            uid = p->pui->i_item[check_item].ui_id;
            gid = p->pui->i_item[check_item].grp_id;
            did = p->pui->i_item[check_item].dev_id;

            memset (dev_resp, 0, sizeof(dev_resp));

            if (!p->pui->i_item[check_item].complete) {
                int status;
                if (!p->pui->i_item[check_item].is_info)
                    ui_set_ritem (p->pfb, p->pui, uid, COLOR_YELLOW, -1);

                status = device_check (p->pui->i_item[check_item].grp_id,
                                    p->pui->i_item[check_item].dev_id, dev_resp);

                printf ("\n%s : gid = %d, did = %d, complete = %d, resp = %s\n",
                        __func__, p->pui->i_item[check_item].grp_id,
                                p->pui->i_item[check_item].dev_id,
                                p->pui->i_item[check_item].complete, dev_resp);

#if defined (__JIG_SELF_MODE__)
                p->pui->i_item[check_item].complete = (dev_resp[0] == 'C') ? 0 : status;
#endif
                client_data_check (p, check_item, dev_resp);
            } else pass_item++;
        }
        // loop delay
        usleep (APP_LOOP_DELAY * 1000);
    }

    // check complete
    RunningTime = 0;    SystemCheckReady = 0;
    printf ("%s : exit!\r\n", __func__);
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
        case 'A':
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
#define UI_ID_IPADDR    4

static void board_ip_file (client_t *p)
{
    int fd;
    struct ifreq ifr;
    char ip_addr[sizeof(struct sockaddr)+1];

    /* this entire function is almost copied from ethtool source code */
    /* Open control socket. */
    ui_set_sitem (p->pfb, p->pui, UI_ID_IPADDR, -1, -1, "???.???.???.???");

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        fprintf (stdout, "Cannot get control socket\n");
        return 0;
    }
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
        fprintf (stdout, "SIOCGIFADDR ioctl Error!!\n");
        close(fd);
        return 0;
    }
    memset (ip_addr, 0x00, sizeof(ip_addr));
    inet_ntop(AF_INET, ifr.ifr_addr.sa_data+2, ip_addr, sizeof(struct sockaddr));

    printf ("%s : ip_address = %s\n", __func__, ip_addr);
    ui_set_sitem (p->pfb, p->pui, UI_ID_IPADDR, -1, -1, ip_addr);
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
    {
        char serial_resp[SERIAL_RESP_SIZE];
        SERIAL_RESP_FORM(serial_resp, 'R', -1, -1, NULL);
        protocol_msg_tx (client.puart, serial_resp);
        protocol_msg_tx (client.puart, "\r\n");
    }

    board_ip_file (&client);

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
