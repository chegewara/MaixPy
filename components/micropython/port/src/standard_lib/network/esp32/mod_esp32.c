#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "py/objtuple.h"
#include "py/objlist.h"
#include "py/stream.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "lib/netutils/netutils.h"
#include "modnetwork.h"
#include "modmachine.h"
#include "mpconfigboard.h"
#include "sleep.h"

#include "esp32_spi.h"
#include "esp32_spi_io.h"
#include "fpioa.h"

typedef struct _esp32_nic_obj_t
{
    mp_obj_base_t base;

} esp32_nic_obj_t;
STATIC bool nic_connected = false;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
STATIC mp_obj_t esp32_nic_ifconfig(mp_obj_t self_in) {
    esp32_spi_net_t* inet = esp32_spi_get_network_data();
	if(inet == NULL)
	{
		return mp_const_none;
	}

	mp_obj_t tuple[3] = {   
        tuple[0] = netutils_format_ipv4_addr( inet->localIp, NETUTILS_BIG ),
        tuple[1] = netutils_format_ipv4_addr( inet->subnetMask, NETUTILS_BIG ),
        tuple[2] = netutils_format_ipv4_addr( inet->gatewayIp, NETUTILS_BIG )
						};

	return mp_obj_new_tuple(3, tuple);
}

STATIC mp_obj_t esp32_nic_isconnected(mp_obj_t self_in) {
    return mp_obj_new_bool(esp32_spi_is_connected());
}

STATIC mp_obj_t esp32_nic_disconnect(mp_obj_t self_in) {
    printf("%s not Supported!\r\n", __func__);
    return mp_const_none;
}

STATIC mp_obj_t esp32_nic_ping(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
	enum { ARG_host, ARG_host_type};
	esp32_nic_obj_t* self = NULL;
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_host, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_host_type, MP_ARG_INT, {.u_obj = mp_const_none} },
        // { MP_QSTR_host_type, MP_ARG_INT, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

	//get nic
	if((mp_obj_type_t*)&mod_network_nic_type_esp32 == mp_obj_get_type(pos_args[0]))
	{
		mp_printf(&mp_plat_print, "[MaixPy] %s | get nic\n",__func__);
		self = pos_args[0];
	}
    // get host
    size_t host_len =0;
    const uint8_t *host = NULL;
	if (args[ARG_host].u_obj != MP_OBJ_NULL) {
        if(mp_obj_get_type(args[ARG_host].u_obj) == &mp_type_str)
            host = mp_obj_str_get_data(args[ARG_host].u_obj, &host_len);
        else if(mp_obj_get_type(args[ARG_host].u_obj) == &mp_type_set){
            host = MP_OBJ_TO_PTR(args[ARG_host].u_obj);
        }
    }
        
    
    // get host type (host name or address)
    int host_type = 1;
	if (args[ARG_host_type].u_int != mp_const_none) {
        host_type = args[ARG_host_type].u_int;
    }
    uint16_t time = esp32_spi_ping((uint8_t*)host, host_type, 100);

    return mp_obj_new_int(time);
}

STATIC mp_obj_t esp32_nic_connect( size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args )
{
	enum { ARG_ssid, ARG_key};
	esp32_nic_obj_t* self = NULL;
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_ssid, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_key, MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

	//get nic
	if((mp_obj_type_t*)&mod_network_nic_type_esp32 == mp_obj_get_type(pos_args[0]))
	{
		mp_printf(&mp_plat_print, "[MaixPy] %s | get nic\n",__func__);
		self = pos_args[0];
	}
    // get ssid
    size_t ssid_len =0;
    const char *ssid = NULL;
	if (args[ARG_ssid].u_obj != mp_const_none) {
        ssid = mp_obj_str_get_data(args[ARG_ssid].u_obj, &ssid_len);
    }		
    // get key
    size_t key_len = 0;
    const char *key = NULL;
    if (args[ARG_key].u_obj != mp_const_none) {
        key = mp_obj_str_get_data(args[1].u_obj, &key_len);
    }
    // connect to AP
    
    int8_t err = esp32_spi_connect_AP((uint8_t*)ssid, (uint8_t*)key, 10);
    if(err == 0)
    	nic_connected = 1;

    return mp_const_none;
}

STATIC mp_obj_t esp32_scan_wifi( mp_obj_t self_in )
{
    mp_obj_t list = mp_obj_new_list(0, NULL);
    char fail_str[30] ;
    bool end;
    int err_code = 0;

    esp32_spi_aps_list_t* aps_list = esp32_spi_scan_networks();
    uint32_t count = aps_list->aps_num;
    if(aps_list == NULL)
        goto err;
    for(int i=0; i<count; i++)
    {
        esp32_spi_ap_t* ap = aps_list->aps[i];
        mp_obj_list_append(list, mp_obj_new_str((char*)ap->ssid, strlen(ap->ssid)));
    }
    aps_list->del(aps_list);
    return list;
err:
    snprintf(fail_str, sizeof(fail_str), "wifi scan fail:%d", err_code);
    mp_raise_msg(&mp_type_OSError, fail_str);
}

STATIC void esp32_make_new_helper(esp32_nic_obj_t *self, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    int cs, rst, rdy, mosi, miso, sclk;

    enum
    {
        ARG_cs,
        ARG_rst,
        ARG_rdy,
        ARG_mosi,
        ARG_miso,
        ARG_sclk,
    };

    static const mp_arg_t allowed_args[] = {
        {MP_QSTR_cs, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_rst, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_rdy, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_mosi, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_miso, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
        {MP_QSTR_sclk, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},
    };

    mp_arg_val_t args_parsed[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args_parsed);

    //cs
    cs = args_parsed[ARG_cs].u_int;
    if (cs == -1 || cs > FUNC_GPIOHS31 || cs < FUNC_GPIOHS0)
    {
        mp_raise_ValueError("gpiohs cs value error!");
    }

    //rst
    rst = args_parsed[ARG_rst].u_int;
    if (rst != -1)//no rst we will use soft reset
    {
        if (rst > FUNC_GPIOHS31 || rst < FUNC_GPIOHS0)
        {
            mp_raise_ValueError("gpiohs rst value error!");
        }
    }

    //rdy
    rdy = args_parsed[ARG_rdy].u_int;
    if (rdy == -1 || rdy > FUNC_GPIOHS31 || rdy < FUNC_GPIOHS0)
    {
        mp_raise_ValueError("gpiohs rdy value error!");
    }

    //mosi
    mosi = args_parsed[ARG_mosi].u_int;
    if (mosi == -1 || mosi > FUNC_GPIOHS31 || mosi < FUNC_GPIOHS0)
    {
        mp_raise_ValueError("gpiohs mosi value error!");
    }

    //miso
    miso = args_parsed[ARG_miso].u_int;
    if (miso == -1 || miso > FUNC_GPIOHS31 || miso < FUNC_GPIOHS0)
    {
        mp_raise_ValueError("gpiohs miso value error!");
    }

    //sclk
    sclk = args_parsed[ARG_sclk].u_int;
    if (sclk == -1 || sclk > FUNC_GPIOHS31 || sclk < FUNC_GPIOHS0)
    {
        mp_raise_ValueError("gpiohs sclk value error!");
    }

    esp32_spi_config_io(cs - FUNC_GPIOHS0, rst, rdy - FUNC_GPIOHS0,
                        mosi - FUNC_GPIOHS0, miso - FUNC_GPIOHS0, sclk - FUNC_GPIOHS0);
    esp32_spi_init();
    mp_printf(&mp_plat_print, "ESP32_SPI init over\r\n");
}

//network.ESP32_SPI(cs=1,rst=2,rdy=3,mosi=4,miso=5,sclk=6)
STATIC mp_obj_t esp32_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    if (n_args != 0 || n_kw != 6)
    {
        mp_raise_ValueError("error argument");
        return mp_const_none;
    }
    esp32_nic_obj_t *self = m_new_obj(esp32_nic_obj_t);
    self->base.type = (mp_obj_type_t *)&mod_network_nic_type_esp32;
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    esp32_make_new_helper(self, n_args, args, &kw_args);
    mod_network_register_nic((mp_obj_t)self);
    return MP_OBJ_FROM_PTR(self);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

STATIC mp_obj_t esp32_adc(mp_obj_t self_in)
{
    #define ESP32_ADC_CH_NUM_TEMP  (ESP32_ADC_CH_NUM)
    uint16_t adc[ESP32_ADC_CH_NUM_TEMP] = {0};//TODO: 6 channel support!!!

    if (esp32_spi_get_adc_val(adc) == 0)
    {
        mp_obj_t *tuple, *tmp;

        tmp = (mp_obj_t *)malloc(ESP32_ADC_CH_NUM_TEMP * sizeof(mp_obj_t));

        for (uint8_t index = 0; index < ESP32_ADC_CH_NUM_TEMP; index++)
            tmp[index] = mp_obj_new_int(adc[index]);

        tuple = mp_obj_new_tuple(ESP32_ADC_CH_NUM_TEMP, tmp);

        free(tmp);
        return tuple;
    }
    else
    {
        mp_raise_ValueError("[MaixPy]: esp32 read adc failed!\r\n");
        return mp_const_false;
    }

    return mp_const_false;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_adc_obj, esp32_adc);

STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_scan_wifi_obj, esp32_scan_wifi);
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(esp32_nic_connect_obj, 1, esp32_nic_connect);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_nic_disconnect_obj, esp32_nic_disconnect);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_nic_isconnected_obj, esp32_nic_isconnected);
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(esp32_nic_ping_obj, 1, esp32_nic_ping);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_nic_ifconfig_obj, esp32_nic_ifconfig);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

STATIC const mp_rom_map_elem_t esp32_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_adc), MP_ROM_PTR(&esp32_adc_obj)},
    {MP_ROM_QSTR(MP_QSTR_scan), MP_ROM_PTR(&esp32_scan_wifi_obj)},
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&esp32_nic_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_disconnect), MP_ROM_PTR(&esp32_nic_disconnect_obj) },  
    { MP_ROM_QSTR(MP_QSTR_isconnected), MP_ROM_PTR(&esp32_nic_isconnected_obj) },
    { MP_ROM_QSTR(MP_QSTR_ifconfig), MP_ROM_PTR(&esp32_nic_ifconfig_obj) },
    { MP_ROM_QSTR(MP_QSTR_ping), MP_ROM_PTR(&esp32_nic_ping_obj) },
    { MP_ROM_QSTR(MP_QSTR_OPEN), MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_WPA_PSK), MP_ROM_INT(2) },
    { MP_ROM_QSTR(MP_QSTR_WPA2_PSK), MP_ROM_INT(3) },
    { MP_ROM_QSTR(MP_QSTR_WPA_WPA2_PSK), MP_ROM_INT(4) },
};

STATIC MP_DEFINE_CONST_DICT(esp32_locals_dict, esp32_locals_dict_table);

STATIC uint8_t sock = 0;

STATIC int esp32_socket_socket(mod_network_socket_obj_t *socket, int *_errno) {
    return 0;
}

STATIC int esp32_socket_connect(mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port, int *_errno) {
	if((mp_obj_type_t*)&mod_network_nic_type_esp32 != mp_obj_get_type(MP_OBJ_TO_PTR(socket->nic)))
	{
		mp_printf(&mp_plat_print, "[MaixPy] %s | esp32_socket_connect can not get nic\n",__func__);
		*_errno = -1;
		return -1;
	}

	switch(socket->u_param.type)
	{
		case MOD_NETWORK_SOCK_STREAM:
		{
            sock = esp32_spi_get_socket();
            int8_t conn = esp32_spi_socket_connect(sock, ip, 0, port, TCP_MODE);
			if(-1 == conn)
			{
				*_errno = -1;
				return -1;
			}
			break;
		}
		case MOD_NETWORK_SOCK_DGRAM:
		{
            mp_printf(&mp_plat_print, "[MaixPy] %s | esp32_socket_connect UDP NOT implemented yet\n",__func__);
            *_errno = -1;
            return -1;
			// break;
		}
		default:
		{
            sock = esp32_spi_get_socket();
            int8_t conn = esp32_spi_socket_connect(sock, ip, 0, port, TCP_MODE);
			if(-1 == conn)
			{
				*_errno = -1;
				return -1;
			}
			break;
		}
	}
    return 0;
}

STATIC mp_uint_t esp32_socket_recv(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, int *_errno) {
	if((mp_obj_type_t*)&mod_network_nic_type_esp32 != mp_obj_get_type(MP_OBJ_TO_PTR(socket->nic)))
	{
		mp_printf(&mp_plat_print, "[MaixPy] %s | esp32_socket_connect can not get nic\n",__func__);
		*_errno = MP_EPIPE;
		return -1;
	}

	uint16_t read_len = 0;
    uint16_t len1 = esp32_spi_socket_available(sock);
    if(len1 == 0)
    {
		*_errno = MP_EIO;
        return read_len;
    }

	read_len = esp32_spi_socket_read(sock, (uint8_t*)buf, len1>len?len:len1);
	if(0 == read_len)
		*_errno = MP_EIO;
	return read_len;
}

STATIC mp_uint_t esp32_socket_send(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, int *_errno) {

	if((mp_obj_type_t*)&mod_network_nic_type_esp32 != mp_obj_get_type(MP_OBJ_TO_PTR(socket->nic)))
	{
		mp_printf(&mp_plat_print, "[MaixPy] %s | esp32_socket_connect can not get nic\n",__func__);
		*_errno = MP_EPIPE;
		return -1;
	}
    int status = esp32_spi_socket_status(sock);
    if(status == SOCKET_ESTABLISHED)

	if(esp32_spi_socket_write(sock, (uint8_t*)buf, len ) == 0)
	{
		mp_printf(&mp_plat_print, "[MaixPy] %s | send data failed\n",__func__);

		*_errno = MP_EIO;
		return -1;
	}
	
    return len;
}

STATIC void esp32_socket_close(mod_network_socket_obj_t *socket) {
	if((mp_obj_type_t*)&mod_network_nic_type_esp32 != mp_obj_get_type(MP_OBJ_TO_PTR(socket->nic)))
	{
		mp_printf(&mp_plat_print, "[MaixPy] %s | esp32_socket_connect can not get nic\n",__func__);
		return ;
	}

    int8_t status = esp32_spi_socket_close(sock);
}

STATIC int esp32_socket_gethostbyname(mp_obj_t nic, const char *name, mp_uint_t len, uint8_t* out_ip) {
	if((mp_obj_type_t*)&mod_network_nic_type_esp32 != mp_obj_get_type(nic))
	{
        return -1;
    }

    uint8_t tmp_ip[6];
    if(esp32_spi_get_host_by_name((uint8_t*)name, tmp_ip) == -1)
        return -1;

    memcpy(out_ip, tmp_ip, 4);
    return true;
}

const mod_network_nic_type_t mod_network_nic_type_esp32 = {
    .base = {
        {&mp_type_type},
        .name = MP_QSTR_ESP32_SPI,
        .make_new = esp32_make_new,
        .locals_dict = (mp_obj_dict_t *)&esp32_locals_dict,
    },
    .gethostbyname = esp32_socket_gethostbyname,
    .connect = esp32_socket_connect,
    .socket = esp32_socket_socket,
    .send = esp32_socket_send,
    .recv = esp32_socket_recv,
    .close = esp32_socket_close,
    /*
    .bind = cc3k_socket_bind,
    .listen = cc3k_socket_listen,
    .accept = cc3k_socket_accept,
    .sendto = cc3k_socket_sendto,
    .recvfrom = cc3k_socket_recvfrom,
    .setsockopt = cc3k_socket_setsockopt,
    .settimeout = cc3k_socket_settimeout,
    .ioctl = cc3k_socket_ioctl,
*/
};
