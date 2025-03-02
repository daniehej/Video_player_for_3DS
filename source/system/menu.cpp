﻿#include "system/headers.hpp"

#include "system/setting_menu.hpp"

#ifdef DEF_ENABLE_SUB_APP0
#include "video_player.hpp"
#endif

#ifdef DEF_ENABLE_SUB_APP1
#include "sub_app1.hpp"
#endif

#ifdef DEF_ENABLE_SUB_APP2
#include "sub_app2.hpp"
#endif

#ifdef DEF_ENABLE_SUB_APP3
#include "sub_app3.hpp"
#endif

#ifdef DEF_ENABLE_SUB_APP4
#include "sub_app4.hpp"
#endif

#ifdef DEF_ENABLE_SUB_APP5
#include "sub_app5.hpp"
#endif

#ifdef DEF_ENABLE_SUB_APP6
#include "sub_app6.hpp"
#endif

#ifdef DEF_ENABLE_SUB_APP7
#include "sub_app7.hpp"
#endif

bool menu_thread_run = false;
bool menu_main_run = true;
bool menu_must_exit = false;
bool menu_check_exit_request = false;
bool menu_update_available = false;
int menu_icon_texture_num[9] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, };
std::string menu_msg[DEF_MENU_NUM_OF_MSG];
Thread menu_worker_thread, menu_send_app_info_thread, menu_check_connectivity_thread, menu_update_thread;
C2D_Image menu_icon_image[10];
Image_data menu_sapp_button[8], menu_sapp_close_button[8], menu_sem_button;

int Menu_check_free_ram(void);
void Menu_get_system_info(void);
void Menu_send_app_info_thread(void* arg);
void Menu_check_connectivity_thread(void* arg);
void Menu_worker_thread(void* arg);
void Menu_update_thread(void* arg);

bool Menu_query_must_exit_flag(void)
{
	return menu_must_exit;
}

void Menu_set_must_exit_flag(bool flag)
{
	menu_must_exit = flag;
}

void Menu_resume(void)
{
	Result_with_string result;

	menu_main_run = true;
	var_need_reflesh = true;
}

void Menu_suspend(void)
{
	menu_main_run = false;
}

Result_with_string Menu_load_msg(std::string lang)
{
	return Util_load_msg("menu_" + lang + ".txt", menu_msg, DEF_MENU_NUM_OF_MSG);
}

void Menu_check_core_thread(void* arg)
{
	threadExit(0);
}

void Menu_init(void)
{
	u8 data = -1;
	u8 region = 0;
	u8 model = 0;
	u32 read_size = 0;
	Thread core_2, core_3;
	Result_with_string result;

	Util_log_init();
	Util_log_save(DEF_MENU_INIT_STR, "Initializing..." + DEF_CURRENT_APP_VER);

	osSetSpeedupEnable(true);
	aptSetSleepAllowed(true);
	svcSetThreadPriority(CUR_THREAD_HANDLE, DEF_THREAD_PRIORITY_HIGH - 1);

	Util_log_save(DEF_MENU_INIT_STR, "fsInit()...", fsInit());
	Util_log_save(DEF_MENU_INIT_STR, "acInit()...", acInit());
	Util_log_save(DEF_MENU_INIT_STR, "aptInit()...", aptInit());
	Util_log_save(DEF_MENU_INIT_STR, "mcuHwcInit()...", mcuHwcInit());
	Util_log_save(DEF_MENU_INIT_STR, "ptmuInit()...", ptmuInit());
	Util_log_save(DEF_MENU_INIT_STR, "httpcInit()...", httpcInit(DEF_HTTP_POST_BUFFER_SIZE));
	Util_log_save(DEF_MENU_INIT_STR, "romfsInit()...", romfsInit());
	Util_log_save(DEF_MENU_INIT_STR, "cfguInit()...", cfguInit());
	Util_log_save(DEF_MENU_INIT_STR, "amInit()...", amInit());
	Util_log_save(DEF_MENU_INIT_STR, "ndspInit()...", ndspInit());//0xd880A7FA
	Util_log_save(DEF_MENU_INIT_STR, "APT_SetAppCpuTimeLimit()...", APT_SetAppCpuTimeLimit(30));

	Util_init();

	//create directory
	Util_file_save_to_file(".", DEF_MAIN_DIR, NULL, 0, false);
	Util_file_save_to_file(".", DEF_MAIN_DIR + "screen_recording/", NULL, 0, false);

	if(Util_file_load_from_file("fake_model.txt", DEF_MAIN_DIR, &data, 1, &read_size).code == 0 && data <= 5)
	{
		var_fake_model = true;
		var_model = data;
	}

	if(CFGU_SecureInfoGetRegion(&region) == 0)
	{
		if(region == CFG_REGION_CHN)
			var_system_region = 1;
		else if(region == CFG_REGION_KOR)
			var_system_region = 2;
		else if(region == CFG_REGION_TWN)
			var_system_region = 3;
		else
			var_system_region = 0;
	}

	if(CFGU_GetSystemModel(&model) == 0)
	{
		Util_log_save(DEF_MENU_INIT_STR, "Model : " + var_model_name[model]);
		if(!var_fake_model)
			var_model = model;
	}
	else
		Util_log_save(DEF_MENU_INIT_STR, "Model : Unknown");

	if(var_fake_model)
		Util_log_save(DEF_MENU_INIT_STR, "Using fake model : " + var_model_name[var_model]);

	if(var_model == CFG_MODEL_2DS || var_model == CFG_MODEL_3DSXL || var_model == CFG_MODEL_3DS)
		osSetSpeedupEnable(false);

	Sem_init();
	Sem_suspend();
	Util_log_save(DEF_MENU_INIT_STR, "Draw_init()...", Draw_init(var_high_resolution_mode, var_3d_mode).code);
	Draw_frame_ready();
	Draw_screen_ready(0, DEF_DRAW_WHITE);
	Draw_screen_ready(1, DEF_DRAW_WHITE);
	Draw_apply_draw();
	Sem_draw_init();

	Util_hid_init();
	Util_expl_init();
	Exfont_init();
	Util_err_init();
	for (int i = 0; i < DEF_EXFONT_NUM_OF_FONT_NAME; i++)
		Exfont_set_external_font_request_state(i, true);

	for(int i = 0; i < 4; i++)
		Exfont_set_system_font_request_state(i, true);

	Exfont_request_load_external_font();
	Exfont_request_load_system_font();

	menu_thread_run = true;
	menu_worker_thread = threadCreate(Menu_worker_thread, (void*)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_REALTIME, 1, false);
	menu_check_connectivity_thread = threadCreate(Menu_check_connectivity_thread, (void*)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_NORMAL, 1, false);
	menu_update_thread = threadCreate(Menu_update_thread, (void*)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_REALTIME, 1, false);

	if(var_model == CFG_MODEL_N2DSXL || var_model == CFG_MODEL_N3DSXL || var_model == CFG_MODEL_N3DS)
	{
		core_2 = threadCreate(Menu_check_core_thread, (void*)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_NORMAL, 2, false);
		if(!core_2)
			var_core_2_available = false;
		else
		{
			threadJoin(core_2, U64_MAX);
			var_core_2_available = true;
		}

		core_3 = threadCreate(Menu_check_core_thread, (void*)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_NORMAL, 3, false);
		if(!core_3)
			var_core_3_available = false;
		else
		{
			threadJoin(core_3, U64_MAX);
			var_core_3_available = true;
		}
		
		threadFree(core_2);
		threadFree(core_3);
	}

	if (var_allow_send_app_info)
		menu_send_app_info_thread = threadCreate(Menu_send_app_info_thread, (void*)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_LOW, 1, true);

	#ifdef DEF_VID_ENABLE_ICON
	menu_icon_texture_num[0] = Draw_get_free_sheet_num();
	result = Draw_load_texture(DEF_VID_ICON_PATH, menu_icon_texture_num[0], menu_icon_image, 0, 1);
	Util_log_save(DEF_MENU_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description, result.code);
	#endif

	#ifdef DEF_SAPP1_ENABLE_ICON
	menu_icon_texture_num[1] = Draw_get_free_sheet_num();
	result = Draw_load_texture(DEF_SAPP1_ICON_PATH, menu_icon_texture_num[1], menu_icon_image, 1, 1);
	Util_log_save(DEF_MENU_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description, result.code);
	#endif

	#ifdef DEF_SAPP2_ENABLE_ICON
	menu_icon_texture_num[2] = Draw_get_free_sheet_num();
	result = Draw_load_texture(DEF_SAPP2_ICON_PATH, menu_icon_texture_num[2], menu_icon_image, 2, 1);
	Util_log_save(DEF_MENU_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description, result.code);
	#endif

	#ifdef DEF_SAPP3_ENABLE_ICON
	menu_icon_texture_num[3] = Draw_get_free_sheet_num();
	result = Draw_load_texture(DEF_SAPP3_ICON_PATH, menu_icon_texture_num[3], menu_icon_image, 3, 1);
	Util_log_save(DEF_MENU_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description, result.code);
	#endif

	#ifdef DEF_SAPP4_ENABLE_ICON
	menu_icon_texture_num[4] = Draw_get_free_sheet_num();
	result = Draw_load_texture(DEF_SAPP4_ICON_PATH, menu_icon_texture_num[4], menu_icon_image, 4, 1);
	Util_log_save(DEF_MENU_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description, result.code);
	#endif

	#ifdef DEF_SAPP5_ENABLE_ICON
	menu_icon_texture_num[5] = Draw_get_free_sheet_num();
	result = Draw_load_texture(DEF_SAPP5_ICON_PATH, menu_icon_texture_num[5], menu_icon_image, 5, 1);
	Util_log_save(DEF_MENU_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description, result.code);
	#endif

	#ifdef DEF_SAPP6_ENABLE_ICON
	menu_icon_texture_num[6] = Draw_get_free_sheet_num();
	result = Draw_load_texture(DEF_SAPP6_ICON_PATH, menu_icon_texture_num[6], menu_icon_image, 6, 1);
	Util_log_save(DEF_MENU_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description, result.code);
	#endif

	#ifdef DEF_SAPP7_ENABLE_ICON
	menu_icon_texture_num[7] = Draw_get_free_sheet_num();
	result = Draw_load_texture(DEF_SAPP7_ICON_PATH, menu_icon_texture_num[7], menu_icon_image, 7, 1);
	Util_log_save(DEF_MENU_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description, result.code);
	#endif

	#ifdef DEF_SEM_ENABLE_ICON
	menu_icon_texture_num[8] = Draw_get_free_sheet_num();
	result = Draw_load_texture(DEF_SEM_ICON_PATH, menu_icon_texture_num[8], menu_icon_image, 8, 2);
	Util_log_save(DEF_MENU_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description, result.code);
	#endif

	for(int i = 0; i < 8; i++)
	{
		menu_sapp_button[i].c2d = var_square_image[0];
		menu_sapp_close_button[i].c2d = var_square_image[0];
	}
	menu_sem_button.c2d = var_square_image[0];

	Menu_get_system_info();

	Menu_resume();
	Util_log_save(DEF_MENU_INIT_STR, "Initialized");
}

void Menu_exit(void)
{
	Util_log_save(DEF_MENU_EXIT_STR, "Exiting...");
	Result_with_string result;

	menu_thread_run = false;

	#ifdef DEF_ENABLE_SUB_APP0
	if (Vid_query_init_flag())
		Vid_exit();
	#endif
	#ifdef DEF_ENABLE_SUB_APP1
	if (Sapp1_query_init_flag())
		Sapp1_exit();
	#endif
	#ifdef DEF_ENABLE_SUB_APP2
	if (Sapp2_query_init_flag())
		Sapp2_exit();
	#endif
	#ifdef DEF_ENABLE_SUB_APP3
	if (Sapp3_query_init_flag())
		Sapp3_exit();
	#endif
	#ifdef DEF_ENABLE_SUB_APP4
	if (Sapp4_query_init_flag())
		Sapp4_exit();
	#endif
	#ifdef DEF_ENABLE_SUB_APP5
	if (Sapp5_query_init_flag())
		Sapp5_exit();
	#endif
	#ifdef DEF_ENABLE_SUB_APP6
	if (Sapp6_query_init_flag())
		Sapp6_exit();
	#endif
	#ifdef DEF_ENABLE_SUB_APP7
	if (Sapp7_query_init_flag())
		Sapp7_exit();
	#endif
	if (Sem_query_init_flag())
		Sem_exit();

	for(int i = 0; i < 8; i++)
		Draw_free_texture(menu_icon_texture_num[i]);

	Util_hid_exit();
	Util_expl_exit();
	Exfont_exit();
	Util_err_exit();

	Util_log_save(DEF_MENU_EXIT_STR, "threadJoin()...", threadJoin(menu_worker_thread, DEF_THREAD_WAIT_TIME));
	Util_log_save(DEF_MENU_EXIT_STR, "threadJoin()...", threadJoin(menu_check_connectivity_thread, DEF_THREAD_WAIT_TIME));
	Util_log_save(DEF_MENU_EXIT_STR, "threadJoin()...", threadJoin(menu_send_app_info_thread, DEF_THREAD_WAIT_TIME));
	Util_log_save(DEF_MENU_EXIT_STR, "threadJoin()...", threadJoin(menu_update_thread, DEF_THREAD_WAIT_TIME));
	threadFree(menu_worker_thread);
	threadFree(menu_check_connectivity_thread);
	threadFree(menu_send_app_info_thread);
	threadFree(menu_update_thread);

	Util_exit();
	fsExit();
	acExit();
	aptExit();
	mcuHwcExit();
	ptmuExit();
	httpcExit();
	romfsExit();
	cfguExit();
	amExit();
	ndspExit();
	Draw_exit();

	Util_log_save(DEF_MENU_EXIT_STR, "Exited.");
}

void Menu_main(void)
{
	int color = DEF_DRAW_BLACK;
	int back_color = DEF_DRAW_WHITE;

	if(var_debug_mode)
		var_need_reflesh = true;

	if (menu_main_run)
	{
		Hid_info key;		
		Util_hid_query_key_state(&key);
		Util_hid_key_flag_reset();

		if(var_need_reflesh || !var_eco_mode)
		{
			var_need_reflesh = false;
			if (var_night_mode)
			{
				color = DEF_DRAW_WHITE;
				back_color = DEF_DRAW_BLACK;
			}

			Draw_frame_ready();
			Draw_screen_ready(0, back_color);

			if(menu_check_exit_request)
			{
				Draw(menu_msg[DEF_MENU_EXIST_MSG], 90.0, 105.0, 0.5, 0.5, color);
				Draw(menu_msg[DEF_MENU_CONFIRM_MSG], 130.0, 140.0, 0.5, 0.5, DEF_DRAW_GREEN);
				Draw(menu_msg[DEF_MENU_CANCEL_MSG], 210.0, 140.0, 0.5, 0.5, DEF_DRAW_RED);
			}
			else if(menu_update_available)
			{
				Draw(menu_msg[DEF_MENU_NEW_VERSION_MSG], 10.0, 30.0, 0.7, 0.7, DEF_DRAW_RED);
				Draw(menu_msg[DEF_MENU_HOW_TO_UPDATE_MSG], 10.0, 60.0, 0.5, 0.5, color);
			}

			if(Util_log_query_log_show_flag())
				Util_log_draw();
	
			Draw_top_ui();

			Draw_screen_ready(1, back_color);

			#ifdef DEF_ENABLE_SUB_APP0
			Draw_texture(&menu_sapp_button[0], menu_sapp_button[0].selected ? DEF_DRAW_AQUA : DEF_DRAW_WEAK_AQUA, 0.0, 0.0, 60.0, 60.0);

			#ifdef DEF_VID_ENABLE_ICON
			Draw_texture(menu_icon_image[0], 0.0, 0.0, 60.0, 60.0);
			#endif
			#ifdef DEF_VID_ENABLE_NAME
			Draw(DEF_VID_NAME, 10.0, 25.0, 0.4, 0.4, color);
			#endif

			if(Vid_query_init_flag())
			{
				Draw_texture(&menu_sapp_close_button[0], menu_sapp_close_button[0].selected ? DEF_DRAW_RED : DEF_DRAW_WEAK_RED, 45.0, 0.0, 15.0, 15.0);
				Draw("X", 47.5, 0.0, 0.5, 0.5, DEF_DRAW_RED);
			}
			#endif
			#ifdef DEF_ENABLE_SUB_APP1
			Draw_texture(&menu_sapp_button[1], menu_sapp_button[1].selected ? DEF_DRAW_AQUA : DEF_DRAW_WEAK_AQUA, 80.0, 0.0, 60.0, 60.0);

			#ifdef DEF_SAPP1_ENABLE_ICON
			Draw_texture(menu_icon_image[1], 80.0, 0.0, 60.0, 60.0);
			#endif
			#ifdef DEF_SAPP1_ENABLE_NAME
			Draw(DEF_SAPP1_NAME, 90.0, 25.0, 0.4, 0.4, color);
			#endif

			if(Sapp1_query_init_flag())
			{
				Draw_texture(&menu_sapp_close_button[1], menu_sapp_close_button[1].selected ? DEF_DRAW_RED : DEF_DRAW_WEAK_RED, 125.0, 0.0, 15.0, 15.0);
				Draw("X", 127.5, 0.0, 0.5, 0.5, DEF_DRAW_RED);
			}
			#endif
			#ifdef DEF_ENABLE_SUB_APP2
			Draw_texture(&menu_sapp_button[2], menu_sapp_button[2].selected ? DEF_DRAW_AQUA : DEF_DRAW_WEAK_AQUA, 160.0, 0.0, 60.0, 60.0);

			#ifdef DEF_SAPP2_ENABLE_ICON
			Draw_texture(menu_icon_image[2], 160.0, 0.0, 60.0, 60.0);
			#endif
			#ifdef DEF_SAPP2_ENABLE_NAME
			Draw(DEF_SAPP2_NAME, 170.0, 25.0, 0.4, 0.4, color);
			#endif

			if(Sapp2_query_init_flag())
			{
				Draw_texture(&menu_sapp_close_button[2], menu_sapp_close_button[2].selected ? DEF_DRAW_RED : DEF_DRAW_WEAK_RED, 205.0, 0.0, 15.0, 15.0);
				Draw("X", 207.5, 0.0, 0.5, 0.5, DEF_DRAW_RED);
			}
			#endif
			#ifdef DEF_ENABLE_SUB_APP3
			Draw_texture(&menu_sapp_button[3], menu_sapp_button[3].selected ? DEF_DRAW_AQUA : DEF_DRAW_WEAK_AQUA, 240.0, 0.0, 60.0, 60.0);

			#ifdef DEF_SAPP3_ENABLE_ICON
			Draw_texture(menu_icon_image[3], 240.0, 0.0, 60.0, 60.0);
			#endif
			#ifdef DEF_SAPP3_ENABLE_NAME
			Draw(DEF_SAPP3_NAME, 250.0, 25.0, 0.4, 0.4, color);
			#endif

			if(Sapp3_query_init_flag())
			{
				Draw_texture(&menu_sapp_close_button[3], menu_sapp_close_button[3].selected ? DEF_DRAW_RED : DEF_DRAW_WEAK_RED, 285.0, 0.0, 15.0, 15.0);
				Draw("X", 287.5, 0.0, 0.5, 0.5, DEF_DRAW_RED);
			}
			#endif
			#ifdef DEF_ENABLE_SUB_APP4
			Draw_texture(&menu_sapp_button[4], menu_sapp_button[4].selected ? DEF_DRAW_AQUA : DEF_DRAW_WEAK_AQUA, 0.0, 80.0, 60.0, 60.0);

			#ifdef DEF_SAPP4_ENABLE_ICON
			Draw_texture(menu_icon_image[4], 0.0, 80.0, 60.0, 60.0);
			#endif
			#ifdef DEF_SAPP4_ENABLE_NAME
			Draw(DEF_SAPP4_NAME, 10.0, 105.0, 0.4, 0.4, color);
			#endif

			if(Sapp4_query_init_flag())
			{
				Draw_texture(&menu_sapp_close_button[4], menu_sapp_close_button[4].selected ? DEF_DRAW_RED : DEF_DRAW_WEAK_RED, 45.0, 80.0, 15.0, 15.0);
				Draw("X", 47.5, 80.0, 0.5, 0.5, DEF_DRAW_RED);
			}
			#endif
			#ifdef DEF_ENABLE_SUB_APP5
			Draw_texture(&menu_sapp_button[5], menu_sapp_button[5].selected ? DEF_DRAW_AQUA : DEF_DRAW_WEAK_AQUA, 80.0, 80.0, 60.0, 60.0);

			#ifdef DEF_SAPP5_ENABLE_ICON
			Draw_texture(menu_icon_image[5], 80.0, 80.0, 60.0, 60.0);
			#endif
			#ifdef DEF_SAPP5_ENABLE_NAME
			Draw(DEF_SAPP5_NAME, 90.0, 105.0, 0.4, 0.4, color);
			#endif

			if(Sapp5_query_init_flag())
			{
				Draw_texture(&menu_sapp_close_button[5], menu_sapp_close_button[5].selected ? DEF_DRAW_RED : DEF_DRAW_WEAK_RED, 125.0, 80.0, 15.0, 15.0);
				Draw("X", 127.5, 80.0, 0.5, 0.5, DEF_DRAW_RED);
			}
			#endif
			#ifdef DEF_ENABLE_SUB_APP6
			Draw_texture(&menu_sapp_button[6], menu_sapp_button[6].selected ? DEF_DRAW_AQUA : DEF_DRAW_WEAK_AQUA, 160.0, 80.0, 60.0, 60.0);

			#ifdef DEF_SAPP6_ENABLE_ICON
			Draw_texture(menu_icon_image[6], 160.0, 80.0, 60.0, 60.0);
			#endif
			#ifdef DEF_SAPP6_ENABLE_NAME
			Draw(DEF_SAPP6_NAME, 170.0, 105.0, 0.4, 0.4, color);
			#endif

			if(Sapp6_query_init_flag())
			{
				Draw_texture(&menu_sapp_close_button[6], menu_sapp_close_button[6].selected ? DEF_DRAW_RED : DEF_DRAW_WEAK_RED, 205.0, 80.0, 15.0, 15.0);
				Draw("X", 207.5, 80.0, 0.5, 0.5, DEF_DRAW_RED);
			}
			#endif
			#ifdef DEF_ENABLE_SUB_APP7
			Draw_texture(&menu_sapp_button[7], menu_sapp_button[7].selected ? DEF_DRAW_AQUA : DEF_DRAW_WEAK_AQUA, 240.0, 80.0, 60.0, 60.0);

			#ifdef DEF_SAPP7_ENABLE_ICON
			Draw_texture(menu_icon_image[7], 240.0, 80.0, 60.0, 60.0);
			#endif
			#ifdef DEF_SAPP7_ENABLE_NAME
			Draw(DEF_SAPP7_NAME, 250.0, 105.0, 0.4, 0.4, color);
			#endif

			if(Sapp7_query_init_flag())
			{
				Draw_texture(&menu_sapp_close_button[7], menu_sapp_close_button[7].selected ? DEF_DRAW_RED : DEF_DRAW_WEAK_RED, 285.0, 80.0, 15.0, 15.0);
				Draw("X", 287.5, 80.0, 0.5, 0.5, DEF_DRAW_RED);
			}
			#endif

			Draw_texture(&menu_sem_button, menu_sem_button.selected ? DEF_DRAW_AQUA : DEF_DRAW_WEAK_AQUA, 260.0, 170.0, 60.0, 60.0);

			#ifdef DEF_SEM_ENABLE_ICON
			Draw_texture(menu_icon_image[8 + var_night_mode], 260.0, 170.0, 60.0, 60.0);
			#endif
			#ifdef DEF_SEM_ENABLE_NAME
			Draw(DEF_SEM_NAME, 270.0, 205.0, 0.4, 0.4, color);
			#endif

			if(Util_err_query_error_show_flag())
				Util_err_draw();

			Draw_bot_ui();

			Draw_apply_draw();
		}
		else
			gspWaitForVBlank();

		if (Util_err_query_error_show_flag())
			Util_err_main(key);
		else
		{
			if(menu_check_exit_request)
			{
				if (key.p_a)
				{
					menu_must_exit = true;
					var_need_reflesh = true;
				}
				else if (key.p_b)
				{
					menu_check_exit_request = false;
					var_need_reflesh = true;
				}
			}
			else
			{
				if(Util_hid_is_pressed(key, *Draw_get_bot_ui_button()))
				{
					Draw_get_bot_ui_button()->selected = true;
					var_need_reflesh = true;
				}
				else if (key.p_start || (Util_hid_is_released(key, *Draw_get_bot_ui_button()) && Draw_get_bot_ui_button()->selected))
				{
					menu_check_exit_request = true;
					var_need_reflesh = true;
				}
				else if (key.p_select)
					Util_log_set_log_show_flag(!Util_log_query_log_show_flag());
				#ifdef DEF_ENABLE_SUB_APP0
				else if (Util_hid_is_pressed(key, menu_sapp_close_button[0]) && Vid_query_init_flag())
				{
					menu_sapp_close_button[0].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_close_button[0]) && Vid_query_init_flag() && menu_sapp_close_button[0].selected)
					Vid_exit();
				else if (Util_hid_is_pressed(key, menu_sapp_button[0]))
				{
					menu_sapp_button[0].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_button[0]) && menu_sapp_button[0].selected)
				{
					if (!Vid_query_init_flag())
						Vid_init();
					else
						Vid_resume();
				}
				#endif
				#ifdef DEF_ENABLE_SUB_APP1
				else if (Util_hid_is_pressed(key, menu_sapp_close_button[1]) && Sapp1_query_init_flag())
				{
					menu_sapp_close_button[1].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_close_button[1]) && Sapp1_query_init_flag() && menu_sapp_close_button[1].selected)
					Sapp1_exit();
				else if (Util_hid_is_pressed(key, menu_sapp_button[1]))
				{
					menu_sapp_button[1].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_button[1]) && menu_sapp_button[1].selected)
				{
					if (!Sapp1_query_init_flag())
						Sapp1_init();
					else
						Sapp1_resume();
				}
				#endif
				#ifdef DEF_ENABLE_SUB_APP2
				else if (Util_hid_is_pressed(key, menu_sapp_close_button[2]) && Sapp2_query_init_flag())
				{
					menu_sapp_close_button[2].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_close_button[2]) && Sapp2_query_init_flag() && menu_sapp_close_button[2].selected)
					Sapp2_exit();
				else if (Util_hid_is_pressed(key, menu_sapp_button[2]))
				{
					menu_sapp_button[2].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_button[2]) && menu_sapp_button[2].selected)
				{
					if (!Sapp2_query_init_flag())
						Sapp2_init();
					else
						Sapp2_resume();
				}
				#endif
				#ifdef DEF_ENABLE_SUB_APP3
				else if (Util_hid_is_pressed(key, menu_sapp_close_button[3]) && Sapp3_query_init_flag())
				{
					menu_sapp_close_button[3].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_close_button[3]) && Sapp3_query_init_flag() && menu_sapp_close_button[3].selected)
					Sapp3_exit();
				else if (Util_hid_is_pressed(key, menu_sapp_button[3]))
				{
					menu_sapp_button[3].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_button[3]) && menu_sapp_button[3].selected)
				{
					if (!Sapp3_query_init_flag())
						Sapp3_init();
					else
						Sapp3_resume();
				}
				#endif
				#ifdef DEF_ENABLE_SUB_APP4
				else if (Util_hid_is_pressed(key, menu_sapp_close_button[4]) && Sapp4_query_init_flag())
				{
					menu_sapp_close_button[4].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_close_button[4]) && Sapp4_query_init_flag() && menu_sapp_close_button[4].selected)
					Sapp4_exit();
				else if (Util_hid_is_pressed(key, menu_sapp_button[4]))
				{
					menu_sapp_button[4].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_button[4]) && menu_sapp_button[4].selected)
				{
					if (!Sapp4_query_init_flag())
						Sapp4_init();
					else
						Sapp4_resume();
				}
				#endif
				#ifdef DEF_ENABLE_SUB_APP5
				else if (Util_hid_is_pressed(key, menu_sapp_close_button[5]) && Sapp5_query_init_flag())
				{
					menu_sapp_close_button[5].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_close_button[5]) && Sapp5_query_init_flag() && menu_sapp_close_button[5].selected)
					Sapp5_exit();
				else if (Util_hid_is_pressed(key, menu_sapp_button[5]))
				{
					menu_sapp_button[5].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_button[5]) && menu_sapp_button[5].selected)
				{
					if (!Sapp5_query_init_flag())
						Sapp5_init();
					else
						Sapp5_resume();
				}
				#endif
				#ifdef DEF_ENABLE_SUB_APP6
				else if (Util_hid_is_pressed(key, menu_sapp_close_button[6]) && Sapp6_query_init_flag())
				{
					menu_sapp_close_button[6].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_close_button[6]) && Sapp6_query_init_flag() && menu_sapp_close_button[6].selected)
					Sapp6_exit();
				else if (Util_hid_is_pressed(key, menu_sapp_button[6]))
				{
					menu_sapp_button[6].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_button[6]) && menu_sapp_button[6].selected)
				{
					if (!Sapp6_query_init_flag())
						Sapp6_init();
					else
						Sapp6_resume();
				}
				#endif
				#ifdef DEF_ENABLE_SUB_APP7
				else if (Util_hid_is_pressed(key, menu_sapp_close_button[7]) && Sapp7_query_init_flag())
				{
					menu_sapp_close_button[7].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_close_button[7]) && Sapp7_query_init_flag() && menu_sapp_close_button[7].selected)
					Sapp7_exit();
				else if (Util_hid_is_pressed(key, menu_sapp_button[7]))
				{
					menu_sapp_button[7].selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sapp_button[7]) && menu_sapp_button[7].selected)
				{
					if (!Sapp7_query_init_flag())
						Sapp7_init();
					else
						Sapp7_resume();
				}
				#endif
				else if (Util_hid_is_pressed(key, menu_sem_button))
				{
					menu_sem_button.selected = true;
					var_need_reflesh = true;
				}
				else if (Util_hid_is_released(key, menu_sem_button) && menu_sem_button.selected)
				{
					if (!Sem_query_init_flag())
						Sem_init();
					else
						Sem_resume();
				}
			}

			if(!key.p_touch && !key.h_touch)
			{
				if(menu_sem_button.selected || Draw_get_bot_ui_button()->selected)
					var_need_reflesh = true;

				for(int i = 0; i < 8; i++)
				{
					if(menu_sapp_button[i].selected || menu_sapp_close_button[i].selected)
						var_need_reflesh = true;

					menu_sapp_button[i].selected = false;
					menu_sapp_close_button[i].selected = false;
				}
				menu_sem_button.selected = false;
				Draw_get_bot_ui_button()->selected = false;
			}
		}

		if(Util_log_query_log_show_flag())
			Util_log_main(key);
	}
	#ifdef DEF_ENABLE_SUB_APP0
	else if (Vid_query_running_flag())
		Vid_main();
	#endif
	#ifdef DEF_ENABLE_SUB_APP1
	else if (Sapp1_query_running_flag())
		Sapp1_main();
	#endif
	#ifdef DEF_ENABLE_SUB_APP2
	else if (Sapp2_query_running_flag())
		Sapp2_main();
	#endif
	#ifdef DEF_ENABLE_SUB_APP3
	else if (Sapp3_query_running_flag())
		Sapp3_main();
	#endif
	#ifdef DEF_ENABLE_SUB_APP4
	else if (Sapp4_query_running_flag())
		Sapp4_main();
	#endif
	#ifdef DEF_ENABLE_SUB_APP5
	else if (Sapp5_query_running_flag())
		Sapp5_main();
	#endif
	#ifdef DEF_ENABLE_SUB_APP6
	else if (Sapp6_query_running_flag())
		Sapp6_main();
	#endif
	#ifdef DEF_ENABLE_SUB_APP7
	else if (Sapp7_query_running_flag())
		Sapp7_main();
	#endif
	else if (Sem_query_running_flag())
		Sem_main();
	else
		menu_main_run = true;
}

void Menu_get_system_info(void)
{
	u8 battery_level = -1;
	u8 battery_voltage = -1;
	char* ssid = (char*)malloc(512);
	Result_with_string result;

	PTMU_GetBatteryChargeState(&var_battery_charge);//battery charge
	result.code = MCUHWC_GetBatteryLevel(&battery_level);//battery level(%)
	if(result.code == 0)
	{
		MCUHWC_GetBatteryVoltage(&battery_voltage);
		var_battery_voltage = 5.0 * (battery_voltage / 256); 
		var_battery_level_raw = battery_level;
	}
	else
	{
		PTMU_GetBatteryLevel(&battery_level);
		if ((int)battery_level == 0)
			var_battery_level_raw = 0;
		else if ((int)battery_level == 1)
			var_battery_level_raw = 5;
		else if ((int)battery_level == 2)
			var_battery_level_raw = 10;
		else if ((int)battery_level == 3)
			var_battery_level_raw = 30;
		else if ((int)battery_level == 4)
			var_battery_level_raw = 60;
		else if ((int)battery_level == 5)
			var_battery_level_raw = 100;
	}

	//ssid
	result.code = ACU_GetSSID(ssid);
	if(result.code == 0)
		var_connected_ssid = ssid;
	else
		var_connected_ssid = "";

	free(ssid);
	ssid = NULL;

	var_wifi_signal = osGetWifiStrength();
	//Get wifi state from shared memory #0x1FF81067
	var_wifi_state = *(u8*)0x1FF81067;
	if(var_wifi_state == 2)
	{
		if (!var_connect_test_succes)
			var_wifi_signal += 4;
	}
	else
	{
		var_wifi_signal = 8;
		var_connect_test_succes = false;
	}

	//Get time
	time_t unixTime = time(NULL);
	struct tm* timeStruct = gmtime((const time_t*)&unixTime);
	var_years = timeStruct->tm_year + 1900;
	var_months = timeStruct->tm_mon + 1;
	var_days = timeStruct->tm_mday;
	var_hours = timeStruct->tm_hour;
	var_minutes = timeStruct->tm_min;
	var_seconds = timeStruct->tm_sec;

	if (var_debug_mode)
	{
		//check free RAM
		var_free_ram = Menu_check_free_ram();
		var_free_linear_ram = linearSpaceFree();
	}

	sprintf(var_status, "%02dfps %04d/%02d/%02d %02d:%02d:%02d ", (int)Draw_query_fps(), var_years, var_months, var_days, var_hours, var_minutes, var_seconds);
}

int Menu_check_free_ram(void)
{
	u8* malloc_check[2000];
	int count;

	for (int i = 0; i < 2000; i++)
		malloc_check[i] = NULL;

	for (count = 0; count < 2000; count++)
	{
		malloc_check[count] = (u8*)malloc(0x186A0);// 100KB
		if (malloc_check[count] == NULL)
			break;
	}

	for (int i = 0; i <= count; i++)
		free(malloc_check[i]);

	return count * 100;//return free KB
}

void Menu_send_app_info_thread(void* arg)
{
	Util_log_save(DEF_MENU_SEND_APP_INFO_STR, "Thread started.");
	OS_VersionBin os_ver;
	bool is_new3ds = false;
	u8* dl_data = NULL;
	u32 status_code = 0;
	u32 downloaded_size = 0;
	char system_ver_char[0x50] = " ";
	std::string new3ds;
	dl_data = (u8*)malloc(0x10000);

	osGetSystemVersionDataString(&os_ver, &os_ver, system_ver_char, 0x50);
	std::string system_ver = system_ver_char;

	APT_CheckNew3DS(&is_new3ds);
	new3ds = is_new3ds ? "yes" : "no";

	std::string send_data = "{ \"app_ver\": \"" + DEF_CURRENT_APP_VER + "\",\"system_ver\" : \"" + system_ver + "\",\"start_num_of_app\" : \"" + std::to_string(var_num_of_app_start) + "\",\"language\" : \"" + var_lang + "\",\"new3ds\" : \"" + new3ds + "\",\"time_to_enter_sleep\" : \"" + std::to_string(var_time_to_turn_off_lcd) + "\",\"scroll_speed\" : \"" + std::to_string(var_scroll_speed) + "\" }";
	Util_httpc_post_and_dl_data(DEF_SEND_APP_INFO_URL, (char*)send_data.c_str(), send_data.length(), dl_data, 0x10000, &downloaded_size, &status_code, true, 5);
	free(dl_data);
	dl_data = NULL;

	Util_log_save(DEF_MENU_SEND_APP_INFO_STR, "Thread exit.");
	threadExit(0);
}

void Menu_check_connectivity_thread(void* arg)
{
	Util_log_save(DEF_MENU_CHECK_INTERNET_STR, "Thread started.");
	u8* http_buffer = NULL;
	u32 status_code = 0;
	u32 dl_size = 0;
	int count = 100;
	http_buffer = (u8*)malloc(0x1000);

	while (menu_thread_run)
	{
		if (count >= 100)
		{
			count = 0;
			Util_httpc_dl_data(DEF_CHECK_INTERNET_URL, http_buffer, 0x1000, &dl_size, &status_code, false, 0);

			if (status_code == 204)
				var_connect_test_succes = true;
			else
				var_connect_test_succes = false;
		}
		else
			usleep(DEF_ACTIVE_THREAD_SLEEP_TIME);

		count++;
	}
	Util_log_save(DEF_MENU_CHECK_INTERNET_STR, "Thread exit.");
	threadExit(0);
}

void Menu_worker_thread(void* arg)
{
	Util_log_save(DEF_MENU_WORKER_THREAD_STR, "Thread started.");
	int count = 0;
	Result_with_string result;

	while (menu_thread_run)
	{
		usleep(49000);
		count++;

		if(count % 5 == 0)
			Menu_get_system_info();

		if (count >= 20)
		{
			var_need_reflesh = true;
			var_afk_time++;
			count = 0;
		}

		if(var_afk_time > var_time_to_turn_off_lcd)
		{
			result = Util_cset_set_screen_state(true, true, false);
			if(result.code != 0)
				Util_log_save(DEF_MENU_WORKER_THREAD_STR, "Util_cset_set_screen_state()..." + result.string + result.error_description, result.code);
		}
		else if(var_afk_time > (var_time_to_turn_off_lcd - 10))
		{
			result = Util_cset_set_screen_brightness(true, true, 10);
			if(result.code != 0)
				Util_log_save(DEF_MENU_WORKER_THREAD_STR, "Util_cset_set_screen_brightness()..." + result.string + result.error_description, result.code);
		}
		else
		{
			result = Util_cset_set_screen_state(true, false, var_turn_on_top_lcd);
			if(result.code != 0)
				Util_log_save(DEF_MENU_WORKER_THREAD_STR, "Util_cset_set_screen_state()..." + result.string + result.error_description, result.code);

			result = Util_cset_set_screen_state(false, true, var_turn_on_bottom_lcd);
			if(result.code != 0)
				Util_log_save(DEF_MENU_WORKER_THREAD_STR, "Util_cset_set_screen_state()..." + result.string + result.error_description, result.code);
			
			if(var_top_lcd_brightness == var_lcd_brightness && var_bottom_lcd_brightness == var_lcd_brightness)
			{
				result = Util_cset_set_screen_brightness(true, true, var_lcd_brightness);
				if(result.code != 0)
					Util_log_save(DEF_MENU_WORKER_THREAD_STR, "Util_cset_set_screen_brightness()..." + result.string + result.error_description, result.code);
			}
			else
			{
				result = Util_cset_set_screen_brightness(true, false, var_top_lcd_brightness);
				if(result.code != 0)
					Util_log_save(DEF_MENU_WORKER_THREAD_STR, "Util_cset_set_screen_brightness()..." + result.string + result.error_description, result.code);
				result = Util_cset_set_screen_brightness(false, true, var_bottom_lcd_brightness);
				if(result.code != 0)
					Util_log_save(DEF_MENU_WORKER_THREAD_STR, "Util_cset_set_screen_brightness()..." + result.string + result.error_description, result.code);
			}
		}

		if (var_flash_mode)
		{
			var_night_mode = !var_night_mode;
			var_need_reflesh = true;
		}
	}
	Util_log_save(DEF_MENU_WORKER_THREAD_STR, "Thread exit.");
	threadExit(0);
}

void Menu_update_thread(void* arg)
{
	Util_log_save(DEF_MENU_UPDATE_THREAD_STR, "Thread started.");
	u8* http_buffer = NULL;
	u32 status_code = 0;
	u32 dl_size = 0;
	size_t pos[2] = { 0, 0, };
	std::string data = "";
	Result_with_string result;
	http_buffer = (u8*)malloc(0x1000);

	result = Util_httpc_dl_data(DEF_CHECK_UPDATE_URL, http_buffer, 0x1000, &dl_size, &status_code, true, 3);
	Util_log_save(DEF_MENU_UPDATE_THREAD_STR, "Util_httpc_dl_data()..." + result.string + result.error_description, result.code);
	if(result.code == 0)
	{
		data = (char*)http_buffer;
		pos[0] = data.find("<newest>");
		pos[1] = data.find("</newest>");
		if(pos[0] != std::string::npos && pos[1] != std::string::npos)
		{
			data = data.substr(pos[0] + 8, pos[1] - (pos[0] + 8));
			if(DEF_CURRENT_APP_VER_INT < atoi(data.c_str()))
				menu_update_available = true;
		}
	}

	Util_log_save(DEF_MENU_UPDATE_THREAD_STR, "Thread exit.");
	threadExit(0);
}