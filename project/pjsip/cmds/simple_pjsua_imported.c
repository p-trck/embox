/**
 * @file
 *
 * Call and accept calls with account registration
 *
 * @date 26.10.2018
 * @author Alexander Kalmuk
 */

#include <pjsua-lib/pjsua.h>

#include <framework/mod/options.h>

#include <mem/heap/mspace_malloc.h>

#include <simple_pjsua_sip_account.inc>

#include <drivers/gpio.h>

#include <pthread.h>

#define THIS_FILE	"APP"

#define PJ_MAX_PORTS 16

#if OPTION_GET(BOOLEAN,use_extern_mem)
#define MM_SET_HEAP(type, p_prev_type) \
	mspace_set_heap(type, p_prev_type)
#else
#define MM_SET_HEAP(type, prev_type) \
	(void) prev_type
#endif

//#define LED_CONTROL
#define LED1_PIN        (1 << 13)
#define LED2_PIN        (1 << 5)
#define PIN_MUTE		(1 << 10)

static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
				pjsip_rx_data *rdata) {
	pjsua_call_info ci;

	pjsua_call_get_info(call_id, &ci);

	PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!!",
			(int)ci.remote_info.slen,
			ci.remote_info.ptr));

	/* Automatically answer incoming calls with 200/OK */
	{
		heap_type_t prev_type;
		MM_SET_HEAP(HEAP_RAM, &prev_type);
		pjsua_call_answer(call_id, 200, NULL, NULL);
		MM_SET_HEAP(prev_type, NULL);
	}
}

static int system_reset(void) {
	printf("System reset...\n");

	/* 모든 인터럽트 비활성화 */
	ipl_t ipl = ipl_save();

    /* CPU 리셋 */
    platform_shutdown(SHUTDOWN_MODE_REBOOT);

	/* 리셋이 실패한 경우 인터럽트 복원 */
	ipl_restore(ipl);

	/* 리셋이 실패하면 이 지점에 도달합니다 */
	fprintf(stderr, "Fail to reset system\n");
	return -1;
}

static int safe_system_reset(void) {
	/* 열린 파일 핸들을 닫고 버퍼를 플러시 */
	fflush(NULL);

	/* 필요한 경우 파일 시스템 동기화 */
	sync();

	/* 잠시 대기 (선택적) */
	sleep(0.2);

	/* 실제 리셋 수행 */
	return system_reset();
}

static void on_call_state(pjsua_call_id call_id, pjsip_event *e) {
	pjsua_call_info ci;

	PJ_UNUSED_ARG(e);

	pjsua_call_get_info(call_id, &ci);
	PJ_LOG(3,(THIS_FILE, "Call %d state=%.*s", call_id,
			 (int)ci.state_text.slen,
			 ci.state_text.ptr));
#ifdef LED_CONTROL
	if(ci.state == PJSIP_INV_STATE_DISCONNECTED)
	{
		gpio_set(GPIO_PORT_J, LED2_PIN, GPIO_PIN_LOW);
	}
	else if (ci.state == PJSIP_INV_STATE_CONFIRMED)
	{
		gpio_set(GPIO_PORT_J, LED2_PIN, GPIO_PIN_HIGH);
	}
#endif
	if(ci.state == PJSIP_INV_STATE_DISCONNECTED)
	{
		// reboot the system
		printf("Rebooting system...\n");
		safe_system_reset();
	}
}

void on_media_event(pjmedia_event *event)
{
	printf("on_media_event: %04x\n", event->type);
}

static void print_available_conf_ports(void) {
	pjsua_conf_port_id id[PJ_MAX_PORTS];
	unsigned port_cnt = PJ_MAX_PORTS;
	pj_status_t status;
	int i;

	status = pjsua_enum_conf_ports(id, &port_cnt);
	assert(status == PJ_SUCCESS);

	printf("\nAvailable conference audio port count: %d\n", port_cnt);

	for (i = 0; i < port_cnt; i++) {
		pjsua_conf_port_info info;
		pjsua_conf_get_port_info(id[i], &info);
		assert(status == PJ_SUCCESS);
		printf("Port(%d): name=%s\n", i, pj_strbuf(&info.name));
	}
	printf("\n");
}

/* Callback called by the library when call's media state has changed */
static void on_call_media_state(pjsua_call_id call_id) {
	pjsua_call_info ci;

	pjsua_call_get_info(call_id, &ci);

	print_available_conf_ports();

	if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
		// When media is active, connect call to sound device.
		pjsua_conf_connect(ci.conf_slot, 0);
		pjsua_conf_connect(0, ci.conf_slot);
	}
}

/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status) {
	pjsua_perror(THIS_FILE, title, status);
	pjsua_destroy();
	MM_SET_HEAP(HEAP_RAM, NULL);
	exit(1);
}

static void init_pjsua(void) {
	pj_status_t status;
	pjsua_config cfg;
	pjsua_logging_config log_cfg;

	pjsua_config_default(&cfg);
	cfg.cb.on_incoming_call = &on_incoming_call;
	cfg.cb.on_call_media_state = &on_call_media_state;
	cfg.cb.on_call_state = &on_call_state;
	cfg.cb.on_media_event = &on_media_event;

	pjsua_logging_config_default(&log_cfg);
	log_cfg.console_level = 1;

	status = pjsua_init(&cfg, &log_cfg, NULL);
	if (status != PJ_SUCCESS) {
		error_exit("pjsua_init() failed", status);
	}
}

static pjsua_transport_id init_udp_transport(void) {
	pjsua_transport_config cfg;
	pj_status_t status;
	pjsua_transport_id transport_id = -1;

	pjsua_transport_config_default(&cfg);
	cfg.port = 5060;
	status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, &transport_id);
	if (status != PJ_SUCCESS) {
		error_exit("Error creating transport", status);
	}

	return transport_id;
}

static void register_acc(pjsua_acc_id *acc_id, pjsua_transport_id t_id) {
	pj_status_t status;

	status = pjsua_acc_add_local(t_id, PJ_TRUE, acc_id);
	if (status != PJ_SUCCESS) {
		error_exit("Error adding account", status);
	}
}

/*
 * main()
 *
 * argv[1] may contain URL to call.
 */
int main(int argc, char *argv[]) {
	pjsua_acc_id acc_id;
	pj_status_t status;
	pjsua_transport_id t_id;

	MM_SET_HEAP(HEAP_EXTERN_MEM, NULL);

	status = pjsua_create();
	if (status != PJ_SUCCESS) {
		error_exit("pjsua_create() failed", status);
	}

	init_pjsua();
	t_id = init_udp_transport();

	status = pjsua_start();
	if (status != PJ_SUCCESS) {
		error_exit("Error starting pjsua", status);
	}

	register_acc(&acc_id, t_id);

	#ifdef LED_CONTROL
	gpio_setup_mode(GPIO_PORT_J, LED1_PIN, GPIO_MODE_OUT);
    gpio_setup_mode(GPIO_PORT_J, LED2_PIN, GPIO_MODE_OUT);
	gpio_set(GPIO_PORT_J, LED1_PIN, GPIO_PIN_HIGH);
	gpio_set(GPIO_PORT_J, LED2_PIN, GPIO_PIN_LOW);
	#endif

	/* If URL is specified, make call to the URL. */
	if (argc > 1) {
		pj_str_t uri = pj_str(argv[1]);

		{
			heap_type_t prev_type;
			MM_SET_HEAP(HEAP_RAM, &prev_type);
			status = pjsua_call_make_call(acc_id, &uri, 0, NULL, NULL, NULL);
			MM_SET_HEAP(prev_type, NULL);
		}

		if (status != PJ_SUCCESS) {
			error_exit("Error making call", status);
		}
	}

	gpio_setup_mode(GPIO_PORT_A, PIN_MUTE, GPIO_MODE_OUT);
	gpio_set(GPIO_PORT_A, PIN_MUTE, GPIO_PIN_LOW);

	#if 0
	puts("PJSIP running..");
	unsigned lv_rx, lv_tx, muted = 0;
	for(;;) {
		pjsua_conf_get_signal_level(0, &lv_tx, &lv_rx);
		if(!muted && lv_tx > 30)
		{
			muted = 1;
			pjsua_conf_adjust_rx_level(0, 0.);
			puts("Mute");
		}
		else if(muted && lv_tx < 30)
		{
			muted = 0;
			pjsua_conf_adjust_rx_level(0, 1.);
			puts("Unmute");
		}
		usleep(100000);
	}
	#endif

	//extern int init_udp_terminal();
	//init_udp_terminal();

	/* Wait until user press "q" to quit. */
	for (;;) {
		char option[10];

		puts("Press 'h' to hangup all calls, 'q' to quit");
		if (fgets(option, sizeof(option), stdin) == NULL) {
			puts("EOF while reading stdin, will quit now..");
			break;
		}

		if (option[0] == 'q')
			break;

		if (option[0] == 'h')
			pjsua_call_hangup_all();
	}

	/* Destroy pjsua */
	pjsua_destroy();
	MM_SET_HEAP(HEAP_RAM, NULL);
	#ifdef LED_CONTROL
	gpio_set(GPIO_PORT_J, LED1_PIN, GPIO_PIN_LOW);
	#endif

	return 0;
}
