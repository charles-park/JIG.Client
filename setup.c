//------------------------------------------------------------------------------
/**
 * @file setup.c
 * @author charles-park (charles.park@hardkernel.com)
 * @brief ODROID client hardware config parse.
 * @version 2.0
 * @date 2025-08-27
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
#include <linux/fb.h>
#include <getopt.h>
#include <pthread.h>

//------------------------------------------------------------------------------
#include "client.h"

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
//------------------------------------------------------------------------------
// return 1 : find success, 0 : not found
//------------------------------------------------------------------------------
#if 0
static int find_file_path (const char *fname, char *file_path)
{
    FILE *fp;
    char cmd_line[STR_PATH_LENGTH * 2];

    memset (cmd_line, 0, sizeof(cmd_line));
    sprintf(cmd_line, "%s\n", "pwd");

    if (NULL != (fp = popen(cmd_line, "r"))) {
        memset (cmd_line, 0, sizeof(cmd_line));
        fgets  (cmd_line, STR_PATH_LENGTH, fp);
        pclose (fp);

        strncpy (file_path, cmd_line, strlen(cmd_line)-1);

        memset (cmd_line, 0, sizeof(cmd_line));
        sprintf(cmd_line, "find -name %s\n", fname);
        if (NULL != (fp = popen(cmd_line, "r"))) {
            memset (cmd_line, 0, sizeof(cmd_line));
            fgets  (cmd_line, STR_PATH_LENGTH, fp);
            pclose (fp);
            if (strlen(cmd_line)) {
                strncpy (&file_path[strlen(file_path)], &cmd_line[1], strlen(cmd_line)-1);
                file_path[strlen(file_path)-1] = 0;
                return 1;
            }
            return 0;
        }
    }
    pclose(fp);
    return 0;
}
#endif

//------------------------------------------------------------------------------
const char *get_model_cmd = "cat /proc/device-tree/model && sync";

static int get_model_name (char *pname)
{
    FILE *fp;
    char *ptr, cmd_line[STR_PATH_LENGTH];

    if (access ("/proc/device-tree/model", F_OK) != 0)
        return 0;

    memset (cmd_line, 0, sizeof(cmd_line));

    if ((fp = popen(get_model_cmd, "r")) != NULL) {
        if (NULL != fgets (cmd_line, sizeof(cmd_line), fp)) {
            if ((ptr = strstr (cmd_line, "ODROID-")) != NULL) {
                strncpy (pname, ptr, strlen(ptr));
                pclose(fp);
                return 1;
            }
        }
        pclose(fp);
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int client_config (client_t *p, const char *model)
{
    FILE *pfd;
    char buf[STR_PATH_LENGTH] = {0,};
    int check_cfg = 0;

    memset (buf, 0, sizeof(buf));

    if (!find_file_path (CLIENT_HW_CONFIG, buf)) {
        printf ("%s : %s file not found!\n", __func__, CLIENT_HW_CONFIG);
        return 0;
    }

    if ((pfd = fopen(buf, "r")) == NULL) {
        printf ("%s : %s file open error!\n", __func__, buf);
        return 0;
    }

    while (fgets(buf, sizeof(buf), pfd) != NULL) {

        if (buf[0] == '#' || buf[0] == '\n')  continue;

        if (!check_cfg) {
            if (strstr(buf, "ODROID-CLIENT-CONFIG") != NULL)    check_cfg = 1;
            continue;
        }

        if (strstr (buf, model) != NULL) {
            char *item;
            // MODEL-NAME(DeviceTree), tty port, tty baud, hdmi fb,
            if (strtok (buf, ",") != NULL) {
                if ((item = strtok (NULL, ",")) != NULL)
                    strncpy (p->uart_dev, item, strlen(item));

                if ((item = strtok (NULL, ",")) != NULL)
                    p->uart_baud = atoi (item);
            }
        }
    }
    fclose (pfd);

    return check_cfg;
}

//------------------------------------------------------------------------------
int client_setup (client_t *p)
{
    char fname [STR_PATH_LENGTH],ui_fname [STR_NAME_LENGTH * 2], dev_fname[STR_NAME_LENGTH * 2];

    memset (fname,     0, sizeof(fname));
    memset (ui_fname,  0, sizeof(ui_fname));
    memset (dev_fname, 0, sizeof(dev_fname));

    if (!get_model_name(p->model))  exit(1);

    tolowerstr (p->model);
    sprintf (ui_fname,  "%s_ui.cfg",  &p->model[strlen("ODROID-")]);
    sprintf (dev_fname, "%s_dev.cfg", &p->model[strlen("ODROID-")]);
    toupperstr (p->model);

    if (!client_config (p, p->model)) {
        memset  (p->uart_dev,    0, STR_NAME_LENGTH);
        sprintf (p->uart_dev, "%s", DEFAULT_CLIENT_UART);

        p->uart_baud = DEFAULT_UART_BAUDRATE;
    }

printf("%s : p->uard_dev = %s, p->uard_baud = %d\n", __func__, p->uart_dev, p->uart_baud);

    if ((p->pfb = fb_init (DEFAULT_CLIENT_FB)) == NULL)        exit(1);

    // lib_dev_check.h???
    find_file_path (ui_fname, fname);
    if ((p->pui = ui_init (p->pfb, fname)) == NULL) exit(1);
    // Default Baudrate (115200 baud)
    if ((p->puart = uart_init (p->uart_dev, p->uart_baud)) != NULL) {
        if (ptc_grp_init (p->puart, 1)) {
            if (!ptc_func_init (p->puart, 0, SERIAL_RESP_SIZE, protocol_check, protocol_catch)) {
                printf ("%s : protocol install error.", __func__);
                exit(1);
            }
        }
        // client device init (lib_dev_check)
        if (!device_setup (dev_fname))  exit(1);

        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------