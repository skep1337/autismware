#include <filesystem>
#include "Elements.h"
#include "Framework/GUI.h"
#include "../Features/Miscellaneous/KitParser.hpp"
#include "../Features/Miscellaneous/SkinChanger.hpp"
#include "../Utils/Config.hpp"
#include "../Features/Visuals/EventLogger.hpp"
#include "../source.hpp"

namespace Menu {

	void DrawMenu() {
		if (GUI::Form::BeginWindow(XorStr("AutismWare")) || GUI::ctx->setup) {
			static int current_weapon = 0;
			static int rage_current_group = 0;
			static int current_group = 0;

			if (GUI::Form::BeginTab(0, XorStr("A")) || GUI::ctx->setup) {

				CVariables::RAGE* rbot = nullptr;
				GUI::Group::BeginGroup(XorStr("Aimbot"), Vector2D(50, 100));
				GUI::Controls::Checkbox(XorStr("Enabled##rage"), &g_Vars.rage.enabled);
				GUI::Controls::Hotkey(XorStr("Enabled key##rage"), &g_Vars.rage.key);

				{
					GUI::Controls::Dropdown(XorStr("Group"), { XorStr("Default"),
						XorStr("Pistols"),
						XorStr("Heavy pistols"),
						XorStr("Rifles"),
						XorStr("AWP"),
						XorStr("Scout"),
						XorStr("Auto snipers"),
						XorStr("Sub machines"),
						XorStr("Heavys"),
						XorStr("Shotguns") }, &rage_current_group);


					switch (rage_current_group - 1) {
					case WEAPONGROUP_PISTOL:
						rbot = &g_Vars.rage_pistols;
						break;
					case WEAPONGROUP_HEAVYPISTOL:
						rbot = &g_Vars.rage_heavypistols;
						break;
					case WEAPONGROUP_SUBMACHINE + 1:
						rbot = &g_Vars.rage_smgs;
						break;
					case WEAPONGROUP_RIFLE:
						rbot = &g_Vars.rage_rifles;
						break;
					case WEAPONGROUP_SHOTGUN + 1:
						rbot = &g_Vars.rage_shotguns;
						break;
					case WEAPONGROUP_SNIPER:
						rbot = &g_Vars.rage_awp;
						break;
					case WEAPONGROUP_SNIPER + 1:
						rbot = &g_Vars.rage_scout;
						break;
					case WEAPONGROUP_HEAVY + 1:
						rbot = &g_Vars.rage_heavys;
						break;
					case WEAPONGROUP_AUTOSNIPER + 1:
						rbot = &g_Vars.rage_autosnipers;
						break;
					default:
						rbot = &g_Vars.rage_default;
						break;
					}

					GUI::Controls::Checkbox(XorStr("Override weapon group") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->active);

					if (rbot->active || GUI::ctx->setup) {
						GUI::Controls::Dropdown(XorStr("Target selection") + std::string(XorStr("#") + std::to_string(rage_current_group)), {
							XorStr("Highest damage"),
							XorStr("Nearest to crosshair"),
							XorStr("Lowest health"),
							XorStr("Lowest distance"),
							XorStr("Lowest latency"), },
							&rbot->target_selection);

						std::vector<MultiItem_t> hitboxes = {
							{ XorStr("Head"), &rbot->hitboxes_head },
							{ XorStr("Chest"), &rbot->hitboxes_chest },
							{ XorStr("Stomach"), &rbot->hitboxes_stomach },
							{ XorStr("Pelvis"), &rbot->hitboxes_pelvis },
							{ XorStr("Arms"), &rbot->hitboxes_arms },
							{ XorStr("Legs"), &rbot->hitboxes_legs },
							{ XorStr("Feet"), &rbot->hitboxes_feets },
						};
						GUI::Controls::MultiDropdown(XorStr("Hitboxes") + std::string(XorStr("#") + std::to_string(rage_current_group)), hitboxes);

						std::vector<MultiItem_t> mp_safety_hitboxes = {
							{ XorStr("Head"), &rbot->mp_hitboxes_head },
							{ XorStr("Chest"), &rbot->mp_hitboxes_chest },
							{ XorStr("Stomach"), &rbot->mp_hitboxes_stomach },
							{ XorStr("Legs"), &rbot->mp_hitboxes_legs },
							{ XorStr("Feet"), &rbot->mp_hitboxes_feets },
						};

						if (!mp_safety_hitboxes.empty())
							GUI::Controls::MultiDropdown(XorStr("Multipoints") + std::string(XorStr("#") + std::to_string(rage_current_group)), mp_safety_hitboxes);

						//if( GUI::Controls::Checkbox( XorStr( "Static pointscale" ) + std::string( XorStr( "#" ) + std::to_string( rage_current_group ) ), &rbot->static_point_scale ) || GUI::ctx->setup ) {

						GUI::Controls::Slider(XorStr("Head scale##687687675") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->point_scale, 0.f, 100.0f, rbot->point_scale <= 0 ? XorStr("Auto") : XorStr("%.0f%%"));
						GUI::Controls::Slider(XorStr("Body scale##68776678") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->body_point_scale, 0.f, 100.0f, rbot->body_point_scale <= 0 ? XorStr("Auto") : XorStr("%.0f%%"));



						GUI::Controls::Checkbox(XorStr("Automatic fire"), &g_Vars.rage.auto_fire);
						GUI::Controls::Checkbox(XorStr("Automatic penetration") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->autowall);
						GUI::Controls::Checkbox(XorStr("Silent aim"), &g_Vars.rage.silent_aim);

						GUI::Controls::Slider(XorStr("Hit chance") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->hitchance, 0.f, 100.f, XorStr("%.0f%%"));

						GUI::Controls::Slider(XorStr("Minimum damage") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->min_damage_visible, 1.f, 130.f, rbot->min_damage_visible > 100 ? (std::string(XorStr("HP+")).append(std::string(std::to_string(rbot->min_damage_visible - 100)))) : XorStr("%dhp"));

						std::vector<MultiItem_t> ovrhitboxes = {
						{ XorStr("Head"), &rbot->hitboxes_ovr_head },
						{ XorStr("Chest"), &rbot->hitboxes_ovr_chest },
						{ XorStr("Stomach"), &rbot->hitboxes_ovr_stomach },
						{ XorStr("Pelvis"), &rbot->hitboxes_ovr_pelvis },
						{ XorStr("Arms"), &rbot->hitboxes_ovr_arms },
						{ XorStr("Legs"), &rbot->hitboxes_ovr_legs },
						{ XorStr("Feet"), &rbot->hitboxes_ovr_feets },
						};

						GUI::Controls::MultiDropdown(XorStr("Hitbox override") + std::string(XorStr("#") + std::to_string(rage_current_group)), ovrhitboxes);
						GUI::Controls::Hotkey(XorStr("Hitbox override#key") + std::string(XorStr("#") + std::to_string(rage_current_group)), &g_Vars.rage.hitbox_override_bind);


						if (GUI::Controls::Checkbox(XorStr("Minimum damage override") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->min_damage_override) || GUI::ctx->setup) {
							GUI::Controls::Hotkey(XorStr("Minimum dmg override key#key") + std::string(XorStr("#") + std::to_string(rage_current_group)), &g_Vars.rage.key_dmg_override);
							GUI::Controls::Slider(XorStr("Dmg override amount##slider") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->min_damage_override_amount, 1.f, 130.f, rbot->min_damage_override_amount > 100 ? (std::string(XorStr("HP+")).append(std::string(std::to_string(rbot->min_damage_override_amount - 100)))) : XorStr("%dhp"));
						}
					}
				}

				GUI::Group::EndGroup();

				GUI::Group::BeginGroup(XorStr("Accuracy"), Vector2D(50, 100)); {

					if (GUI::Controls::Checkbox(XorStr("Extended backtrack") + std::string(XorStr("#") + std::to_string(rage_current_group)), &g_Vars.misc.extended_backtrack)) {
						GUI::Controls::Hotkey(XorStr("Extended backtrack qswdecxvrftgbyhnujmiko,l") + std::string(XorStr("#") + std::to_string(rage_current_group)), &g_Vars.misc.extended_backtrack_key);
						GUI::Controls::Slider(XorStr("Extended backtrack##amt") + std::string(XorStr("#") + std::to_string(rage_current_group)), &g_Vars.misc.extended_backtrack_time, 0.f, 1.f, XorStr("%.2fs"), 0.1f);
					}

					//if( !g_Vars.misc.anti_untrusted || GUI::ctx->setup )


					GUI::Controls::Checkbox(XorStr("Auto stop"), &rbot->autostop_check);

					GUI::Controls::Checkbox(XorStr("Automatic scope") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->autoscope);

					GUI::Controls::Checkbox(XorStr("Prefer baim") + std::string(XorStr("#") + std::to_string(rage_current_group)), &rbot->prefer_body);

					GUI::Controls::Checkbox(XorStr("Knife bot"), &g_Vars.misc.knife_bot);
					if (g_Vars.misc.knife_bot || GUI::ctx->setup)
						GUI::Controls::Dropdown(XorStr("Knifebot type##Knife bot type"), { XorStr("Default"), XorStr("Backstab"), XorStr("Quick") }, &g_Vars.misc.knife_bot_type);

					GUI::Controls::Checkbox(XorStr("Zeus bot"), &g_Vars.misc.zeus_bot);
					if (g_Vars.misc.zeus_bot || GUI::ctx->setup)
						GUI::Controls::Slider(XorStr("Zeus bot hitchance"), &g_Vars.misc.zeus_bot_hitchance, 1.f, 80.f, XorStr("%.0f%%"));

				} GUI::Group::EndGroup();

			}

			if (GUI::Form::BeginTab(1, XorStr("B")) || GUI::ctx->setup)
			{
				GUI::Group::BeginGroup(("Antiaim"), Vector2D(50, 100));
				{
					CVariables::ANTIAIM_STATE* settings = &g_Vars.antiaim_stand;
					// enable AA.
					if (GUI::Controls::Checkbox(("Enabled##aa"), &g_Vars.antiaim.enabled))
						// yaw / pitch / fake.
						GUI::Controls::Dropdown(("Pitch"), { ("Off"), ("Down"), ("Up"), ("Zero") }, &settings->pitch);
					GUI::Controls::Dropdown(("Yaw"), { ("Off"), ("Backwards"), ("180z"), "Jitter" }, &settings->base_yaw);


					if (settings->base_yaw > 0)
						GUI::Controls::Slider(("Yaw addition"), &g_Vars.antiaim.yaw_addition, -180, 180);

					if (settings->base_yaw == 3)
					{
						GUI::Controls::Dropdown(("Jitter mode"), { ("Random"), ("Center") }, &settings->jitter_mode);
						GUI::Controls::Slider(("Jitter amount"), &g_Vars.antiaim.random_jitter, -360, 360);
					}

					GUI::Controls::Dropdown(("Fake yaw"), { ("Off"), ("Default"), ("Flicker") }, & settings->fake_yaw);
					GUI::Controls::Dropdown(("Body yaw"), { ("Off"), ("Twist"), ("Static"), ("Break Logic") }, &settings->yaw);

					if (settings->yaw > 0 && settings->yaw != 3) {
						GUI::Controls::Slider(("Delta"), &g_Vars.antiaim.break_lby, -179, 179);
					}

					if (settings->base_yaw == 0) {}

					GUI::Controls::Checkbox(("Freestanding"), &g_Vars.antiaim.freestand);

					GUI::Controls::Checkbox(("Draw angles"), &g_Vars.antiaim.draw_angles);

					if (GUI::Controls::Checkbox(("Manual"), &g_Vars.antiaim.manual) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(("Manual color"), &g_Vars.antiaim.manual_color);
						GUI::Controls::Label(("Left"));
						GUI::Controls::Hotkey(("Left key##key"), &g_Vars.antiaim.manual_left_bind, false);

						GUI::Controls::Label(("Right"));
						GUI::Controls::Hotkey(("Right key##key"), &g_Vars.antiaim.manual_right_bind, false);

						GUI::Controls::Label(("Back"));
						GUI::Controls::Hotkey(("Back key##key"), &g_Vars.antiaim.manual_back_bind, false);
					}

					// distortion.
					if (GUI::Controls::Checkbox(("Distortion###sse"), &g_Vars.antiaim.distort)) {
						// manual aa is on, show this.
						if (g_Vars.antiaim.manual)
							GUI::Controls::Checkbox(("Manual override"), &g_Vars.antiaim.distort_manual_aa);
						GUI::Controls::Checkbox(("Spin"), &g_Vars.antiaim.distort_spin);
						GUI::Controls::Slider(("Speed"), &g_Vars.antiaim.distort_speed, 1.f, 10.f, ("%.1f"));
						GUI::Controls::Slider(("Range"), &g_Vars.antiaim.distort_range, 0.f, 360.f);

						std::vector<MultiItem_t> distort_disablers = {
							{ ("Fakewalking"), &g_Vars.antiaim.distort_disable_fakewalk },
						{ ("Running"), &g_Vars.antiaim.distort_disable_run },
						{ ("Airborne"), &g_Vars.antiaim.distort_disable_air },
						};

						GUI::Controls::MultiDropdown(("Distortion disablers"), distort_disablers);
					}

					if (GUI::Controls::Checkbox(("Fake-walk"), &g_Vars.misc.slow_walk)) {
						GUI::Controls::Hotkey(("Fake-walk key#Key"), &g_Vars.misc.slow_walk_bind);
					}


				}
				GUI::Group::EndGroup();

				GUI::Group::BeginGroup(XorStr("Fake lag"), Vector2D(50, 50)); {
					GUI::Controls::Checkbox(XorStr("Enabled##fakeKURWAlag"), &g_Vars.fakelag.enabled);

					std::vector<MultiItem_t> fakelag_cond = {
						{ XorStr("Moving"), &g_Vars.fakelag.when_moving },
						{ XorStr("In air"), &g_Vars.fakelag.when_air },
						{ XorStr("On Peek"), &g_Vars.fakelag.trigger_on_peek },
						{ XorStr("LBY Update"), &g_Vars.fakelag.lby_update },
						{ XorStr("Break LC"), &g_Vars.fakelag.break_lag_compensation },
					};
					GUI::Controls::MultiDropdown(XorStr("Conditions"), fakelag_cond);
					GUI::Controls::Dropdown(XorStr("Type"), { XorStr("Maximum"), XorStr("Dynamic"), XorStr("Fluctuate") }, &g_Vars.fakelag.choke_type);
					GUI::Controls::Slider(XorStr("Limit"), &g_Vars.fakelag.choke, 2, 14);
					GUI::Controls::Slider(XorStr("Alternate Limit"), &g_Vars.fakelag.alternative_choke, 2, 14);

					g_Vars.fakelag.trigger_duck = false;
					g_Vars.fakelag.trigger_weapon_activity = false; //g_Vars.fakelag.trigger_shooting =
					g_Vars.fakelag.trigger_reloading = false;
					g_Vars.fakelag.trigger_land = true;

					GUI::Controls::Slider(XorStr("Variance"), &g_Vars.fakelag.variance, 0.0f, 100.0f, XorStr("%.0f %%"));

				}
				GUI::Group::EndGroup();

				GUI::Group::BeginGroup(XorStr("Exploits"), Vector2D(50, 50)); {
				}
				GUI::Group::EndGroup();
			}

			if (GUI::Form::BeginTab(2, XorStr("C")) || GUI::ctx->setup) {

				GUI::Group::BeginGroup(XorStr("General"), Vector2D(50, 50));
				{
					GUI::Controls::Checkbox(XorStr("Enabled"), &g_Vars.esp.esp_enable);
					g_Vars.esp.team_check = true;

					GUI::Controls::Checkbox(XorStr("Dormant"), &g_Vars.esp.fade_esp);
					//GUI::Controls::ColorPicker( XorStr( "dormant clr" ), &g_Vars.esp.dormant_color );

					GUI::Controls::Checkbox(XorStr("Box"), &g_Vars.esp.box);
					GUI::Controls::ColorPicker(XorStr("Box color"), &g_Vars.esp.box_color);

					if (GUI::Controls::Checkbox(XorStr("Offscreen"), &g_Vars.esp.offscren_enabled) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(XorStr("Offscreen color"), &g_Vars.esp.offscreen_color);
						GUI::Controls::Slider(XorStr("Offscreen distance"), &g_Vars.esp.offscren_distance, 10, 100.f, XorStr("%.0f%%"));
						GUI::Controls::Slider(XorStr("Offscreen size"), &g_Vars.esp.offscren_size, 4, 16.f, XorStr("%.0fpx"));
					}

					GUI::Controls::Checkbox(XorStr("Name"), &g_Vars.esp.name);
					GUI::Controls::ColorPicker(XorStr("Name color"), &g_Vars.esp.name_color);

					GUI::Controls::Checkbox(XorStr("Health"), &g_Vars.esp.health);
					GUI::Controls::Checkbox(XorStr("Health color override"), &g_Vars.esp.health_override);
					GUI::Controls::ColorPicker(XorStr("Health color override color##Health"), &g_Vars.esp.health_color);

					GUI::Controls::Checkbox(XorStr("Skeleton"), &g_Vars.esp.skeleton);
					GUI::Controls::ColorPicker(XorStr("Skeleton color##Skelet Color"), &g_Vars.esp.skeleton_color);

					GUI::Controls::Checkbox(XorStr("Ammo"), &g_Vars.esp.draw_ammo_bar);
					GUI::Controls::ColorPicker(XorStr("Ammo color##color"), &g_Vars.esp.ammo_color);

					GUI::Controls::Checkbox(XorStr("LBY timer"), &g_Vars.esp.draw_lby_bar);
					GUI::Controls::ColorPicker(XorStr("LBY timer color##color"), &g_Vars.esp.lby_color);

					GUI::Controls::Checkbox(XorStr("Weapon"), &g_Vars.esp.weapon);
					GUI::Controls::ColorPicker(XorStr("Weapon color##color"), &g_Vars.esp.weapon_color);

					GUI::Controls::Checkbox(XorStr("Weapon icon"), &g_Vars.esp.weapon_icon);
					GUI::Controls::ColorPicker(XorStr("Weapon icon color##color"), &g_Vars.esp.weapon_icon_color);

					std::vector<MultiItem_t> flags = {
						{ XorStr("Zoom"), &g_Vars.esp.draw_scoped },
						{ XorStr("Flashed"), &g_Vars.esp.draw_flashed },
						{ XorStr("Money"), &g_Vars.esp.draw_money },
						{ XorStr("Kevlar"), &g_Vars.esp.draw_armor },
						{ XorStr("Bomb"), &g_Vars.esp.draw_bombc4 },
						{ XorStr("Defusing"), &g_Vars.esp.draw_defusing },
						{ XorStr("Distance"), &g_Vars.esp.draw_distance },
						{ XorStr("Grenade pin"), &g_Vars.esp.draw_grenade_pin },
						{ XorStr("Resolver mode"), &g_Vars.esp.draw_resolver },
					};

					GUI::Controls::MultiDropdown(XorStr("Flags"), flags);

					std::vector<MultiItem_t> hitmarkers = {
						{ XorStr("World"), &g_Vars.esp.visualize_hitmarker_world },
						{ XorStr("Screen"), &g_Vars.esp.vizualize_hitmarker },
					};

					GUI::Controls::MultiDropdown(XorStr("Hitmarkers"), hitmarkers);
					GUI::Controls::Checkbox(XorStr("Hitsound"), &g_Vars.misc.hitsound);
					GUI::Controls::Dropdown(XorStr("Hitsound type"), { XorStr("Default"), XorStr("Custom") }, &g_Vars.misc.hitsound_type);

					if (g_Vars.misc.hitsound_type || GUI::ctx->setup) {
						const auto GetScripts = []() {
							std::string dir = GetDocumentsDirectory().append(XorStr("\\ams\\"));
							for (auto& file_path : std::filesystem::directory_iterator(dir)) {
								if (!file_path.path().string().empty()) {
									if (file_path.path().string().find(XorStr(".wav")) != std::string::npos) {
										g_Vars.globals.m_hitsounds.emplace_back(file_path.path().string().erase(0, dir.length()));
									}
								}
							}
						};

						if (g_Vars.globals.m_hitsounds.empty()) {
							GetScripts();
						}

						if (g_Vars.globals.m_hitsounds.empty()) {
							GUI::Controls::Dropdown(XorStr("Custom hitsounds"), { XorStr("None: Documents\\ams\\*.wav") }, &g_Vars.misc.hitsound_custom);
						}
						else {
							GUI::Controls::Dropdown(XorStr("Custom hitsounds"), g_Vars.globals.m_hitsounds, &g_Vars.misc.hitsound_custom);
							GUI::Controls::Slider(XorStr("Sound volume"), &g_Vars.misc.hitsound_volume, 1.f, 100.f, XorStr("%.0f%%"));
						}

						GUI::Controls::Button(XorStr("Refresh sounds"), [&]() -> void {
							g_Vars.globals.m_hitsounds.clear();

							GetScripts();
							});
					}

					GUI::Group::EndGroup();
				}

				GUI::Group::BeginGroup(XorStr("Player models"), Vector2D(50, 50));
				{
					std::vector<std::string> materials = {
						XorStr("Off"), XorStr("Flat"), XorStr("Material"), XorStr("Ghost"), XorStr("Outline")
					};

					if (GUI::Controls::Checkbox(XorStr("Enemy chams"), &g_Vars.esp.chams_enemy) || GUI::ctx->setup) {
						GUI::Controls::Checkbox(XorStr("Visible chams"), &g_Vars.esp.enemy_chams_vis);
						GUI::Controls::ColorPicker(XorStr("Visible chams color##Local color"), &g_Vars.esp.enemy_chams_color_vis);

						GUI::Controls::Checkbox(XorStr("Behind wall chams"), &g_Vars.esp.enemy_chams_xqz);
						GUI::Controls::ColorPicker(XorStr("Behind wall chams color##Local color"), &g_Vars.esp.enemy_chams_color_xqz);

						GUI::Controls::Dropdown(XorStr("Enemy chams material"), {
							XorStr("Off"), XorStr("Flat"), XorStr("Material"), XorStr("Ghost"), XorStr("Outline"), XorStr("Shiny")
							}, &g_Vars.esp.enemy_chams_mat);

						if (g_Vars.esp.enemy_chams_mat == 5) {
							GUI::Controls::Slider(XorStr("Enemy pearlescence"), &g_Vars.esp.chams_enemy_pearlescence, 0.f, 100.f, XorStr("%0.0f"));
							GUI::Controls::ColorPicker(XorStr("clacoenemycascans"), &g_Vars.esp.chams_enemy_pearlescence_color);
							GUI::Controls::Slider(XorStr("Enemy shine"), &g_Vars.esp.chams_enemy_shine, 0.f, 100.f, XorStr("%0.0f"));
						}

						GUI::Controls::Checkbox(XorStr("Glow"), &g_Vars.esp.glow_enemy);
						GUI::Controls::ColorPicker(XorStr("Glow color#Enemy glow"), &g_Vars.esp.glow_enemy_color);

						if (GUI::Controls::Checkbox(XorStr("Add glow#Enemy"), &g_Vars.esp.chams_enemy_outline) || GUI::ctx->setup) {
							GUI::Controls::ColorPicker(XorStr("Add glow color#Enemy"), &g_Vars.esp.chams_enemy_outline_color);
							GUI::Controls::Checkbox(XorStr("Wireframe#Enemy"), &g_Vars.esp.chams_enemy_outline_wireframe);

							GUI::Controls::Slider(XorStr("Glow strength#Enemy"), &g_Vars.esp.chams_enemy_outline_value, 1.f, 100.f, XorStr("%0.0f"));
						}
					}
					if (GUI::Controls::Checkbox(XorStr("Backtrack chams"), &g_Vars.esp.chams_history) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(XorStr("Backtrack chams color##Local color"), &g_Vars.esp.chams_history_color);

						GUI::Controls::Dropdown(XorStr("Backtrack chams material"), materials, &g_Vars.esp.chams_history_mat);
					}
					if (GUI::Controls::Checkbox(XorStr("Shot chams"), &g_Vars.esp.hitmatrix) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(XorStr("Shot chams color##Local color"), &g_Vars.esp.hitmatrix_color);

						GUI::Controls::Slider(XorStr("Expire time##chams"), &g_Vars.esp.hitmatrix_time, 1.f, 10.f, XorStr("%0.0f seconds"));
					}

					if (GUI::Controls::Checkbox(XorStr("Local glow"), &g_Vars.esp.glow_local) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(XorStr("Local glow color##Local color"), &g_Vars.esp.glow_local_color);
					}
					if (GUI::Controls::Checkbox(XorStr("Local chams"), &g_Vars.esp.chams_local) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(XorStr("Local chams color##Local color"), &g_Vars.esp.chams_local_color);

						GUI::Controls::Dropdown(XorStr("Local chams material"), {
							XorStr("Off"), XorStr("Flat"), XorStr("Material"), XorStr("Ghost"), XorStr("Outline"), XorStr("Shiny")
							}, &g_Vars.esp.chams_local_mat);
						if (g_Vars.esp.chams_local_mat == 5) {
							GUI::Controls::Slider(XorStr("Local pearlescence"), &g_Vars.esp.chams_local_pearlescence, 0.f, 100.f, XorStr("%0.0f"));
							GUI::Controls::ColorPicker(XorStr("clacolacscascans"), &g_Vars.esp.chams_local_pearlescence_color);
							GUI::Controls::Slider(XorStr("Local shine"), &g_Vars.esp.chams_local_shine, 0.f, 100.f, XorStr("%0.0f"));
						}

						if (GUI::Controls::Checkbox(XorStr("Add glow local#Local"), &g_Vars.esp.chams_local_outline) || GUI::ctx->setup) {
							GUI::Controls::ColorPicker(XorStr("Add glow local color#Local"), &g_Vars.esp.chams_local_outline_color);
							GUI::Controls::Checkbox(XorStr("Wireframe local#Local"), &g_Vars.esp.chams_local_outline_wireframe);

							GUI::Controls::Slider(XorStr("Local glow strength#Local"), &g_Vars.esp.chams_local_outline_value, 1.f, 100.f, XorStr("%0.0f"));
						}
					}

					GUI::Controls::Checkbox(XorStr("Transparency in scope"), &g_Vars.esp.blur_in_scoped);
					GUI::Controls::Slider(XorStr("Transparency##Transparency In Scope"), &g_Vars.esp.blur_in_scoped_value, 0.0f, 100.f, XorStr("%0.f%%"));

					if (GUI::Controls::Checkbox(XorStr("Attachment chams"), &g_Vars.esp.chams_attachments) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(XorStr("Attachment chams color"), &g_Vars.esp.attachments_chams_color);
						GUI::Controls::Dropdown(XorStr("Attachment chams material"), materials, &g_Vars.esp.attachments_chams_mat);

						if (GUI::Controls::Checkbox(XorStr("Add glow attachment#Attachment"), &g_Vars.esp.chams_attachments_outline) || GUI::ctx->setup) {
							GUI::Controls::ColorPicker(XorStr("Add glow attachment color#Attachment"), &g_Vars.esp.chams_attachments_outline_color);
							GUI::Controls::Checkbox(XorStr("Wireframe attachment#Attachment"), &g_Vars.esp.chams_attachments_outline_wireframe);

							GUI::Controls::Slider(XorStr("Attachment glow strength#Attachment"), &g_Vars.esp.chams_attachments_outline_value, 1.f, 100.f, XorStr("%0.0f"));
						}
					}

					if (GUI::Controls::Checkbox(XorStr("Weapon chams"), &g_Vars.esp.chams_weapon) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(XorStr("Weapon chams color"), &g_Vars.esp.weapon_chams_color);
						GUI::Controls::Dropdown(XorStr("Weapon chams material"), materials, &g_Vars.esp.weapon_chams_mat);

						if (GUI::Controls::Checkbox(XorStr("Add glow weapon#Weapon"), &g_Vars.esp.chams_weapon_outline) || GUI::ctx->setup) {
							GUI::Controls::ColorPicker(XorStr("Add glow weapon color#Weapon"), &g_Vars.esp.chams_weapon_outline_color);
							GUI::Controls::Checkbox(XorStr("Wireframe weapon#Weapon"), &g_Vars.esp.chams_weapon_outline_wireframe);

							GUI::Controls::Slider(XorStr("Weapon glow strength#Weapon"), &g_Vars.esp.chams_weapon_outline_value, 1.f, 100.f, XorStr("%0.0f"));
						}
					}

					if (GUI::Controls::Checkbox(XorStr("Hand chams"), &g_Vars.esp.chams_hands) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(XorStr("Hand chams color"), &g_Vars.esp.hands_chams_color);
						GUI::Controls::Dropdown(XorStr("Hand chams material"), materials, &g_Vars.esp.hands_chams_mat);

						if (GUI::Controls::Checkbox(XorStr("Add glow hand#Hand"), &g_Vars.esp.chams_hands_outline) || GUI::ctx->setup) {
							GUI::Controls::ColorPicker(XorStr("Add glow hand color#Hand"), &g_Vars.esp.chams_hands_outline_color);
							GUI::Controls::Checkbox(XorStr("Wireframe hand#Hand"), &g_Vars.esp.chams_hands_outline_wireframe);

							GUI::Controls::Slider(XorStr("Hand glow strength#Hand"), &g_Vars.esp.chams_hands_outline_value, 1.f, 100.f, XorStr("%0.0f"));
						}
					}

					if (GUI::Controls::Checkbox(XorStr("Shot capsules"), &g_Vars.esp.draw_hitboxes)) {
						GUI::Controls::ColorPicker(XorStr("Shot capsules color"), &g_Vars.esp.hitboxes_color);
					}

					GUI::Group::EndGroup();
				}

				GUI::Group::BeginGroup(XorStr("Other ESP"), Vector2D(50, 50));
				{
					if (GUI::Controls::Checkbox(XorStr("Anim layer info"), &g_Vars.esp.draw_layer_info)) {
						GUI::Controls::Dropdown(XorStr("Layer"), {
							XorStr("1"),
							XorStr("2"),
							XorStr("3"),
							XorStr("4"),
							XorStr("5"),
							XorStr("6"),
							XorStr("7"),
							XorStr("8"),
							XorStr("9"),
							XorStr("10"),
							XorStr("11"),
							XorStr("12"),
							XorStr("13"), }, &g_Vars.esp.layer);
					}


					if (GUI::Controls::Checkbox(XorStr("Preserve killfeed"), &g_Vars.esp.preserve_killfeed)) {
						// note - alpha;
						// maybe make a dropdown, with options such as:
						// "extend", "preserve" where if u have extend 
						// you can choose how long, and if u have preserve
						// just force this to FLT_MAX or smth? idk.
						g_Vars.esp.preserve_killfeed_time = 300.f;
					}

					GUI::Controls::Checkbox(XorStr("Visualize damage"), &g_Vars.esp.visualize_damage);
					GUI::Controls::Checkbox(XorStr("Disable post processing"), &g_Vars.esp.remove_post_proccesing);
					GUI::Controls::Checkbox(XorStr("Taser range"), &g_Vars.esp.zeus_distance);
					GUI::Controls::ColorPicker(XorStr("Taser range color"), &g_Vars.esp.zeus_distance_color);
					GUI::Controls::Checkbox(XorStr("Keybind list"), &g_Vars.esp.keybind_window_enabled);
					GUI::Controls::Checkbox(XorStr("Spectator list"), &g_Vars.esp.spec_window_enabled);

					GUI::Controls::Checkbox(XorStr("Radar"), &g_Vars.misc.ingame_radar);
					GUI::Controls::Checkbox(XorStr("Penetration crosshair"), &g_Vars.esp.autowall_crosshair);
					GUI::Controls::Checkbox(XorStr("Force crosshair"), &g_Vars.esp.force_sniper_crosshair);

					GUI::Controls::Checkbox(XorStr("Client bullet impacts"), &g_Vars.misc.impacts_spoof);
					GUI::Controls::ColorPicker(XorStr("Client bullet impacts color"), &g_Vars.esp.client_impacts);

					GUI::Controls::Checkbox(XorStr("Server bullet impacts"), &g_Vars.misc.server_impacts_spoof);
					GUI::Controls::ColorPicker(XorStr("Server bullet impacts color"), &g_Vars.esp.server_impacts);

					GUI::Controls::Checkbox(XorStr("Bomb"), &g_Vars.esp.draw_c4_bar);
					GUI::Controls::ColorPicker(XorStr("Bomb color"), &g_Vars.esp.c4_color);

					std::vector<MultiItem_t> wwweaponnnnn_n = {
						{ XorStr("Text"), &g_Vars.esp.dropped_weapons },
						{ XorStr("Ammo"), &g_Vars.esp.dropped_weapons_ammo },
					};

					GUI::Controls::MultiDropdown(XorStr("Dropped weapons"), wwweaponnnnn_n);
					GUI::Controls::ColorPicker(XorStr("Dropped weapons color"), &g_Vars.esp.dropped_weapons_color);

					GUI::Controls::Checkbox(XorStr("Grenades"), &g_Vars.esp.nades);

					if (GUI::Controls::Checkbox(XorStr("Grenade prediction"), &g_Vars.esp.NadePred) || GUI::ctx->setup) {
						GUI::Controls::ColorPicker(XorStr("Grenade prediction color"), &g_Vars.esp.nade_pred_color);
					}

					GUI::Controls::Slider(XorStr("Override FOV"), &g_Vars.esp.world_fov, 0.f, 200.f, XorStr("%.0f degress"));
					GUI::Controls::Slider(XorStr("Override Viewmodel FOV "), &g_Vars.misc.viewmodel_fov, 0.f, 200.f, XorStr("%.0f degress"));

					if (GUI::Controls::Checkbox(XorStr("Aspect ratio"), &g_Vars.esp.aspect_ratio) || GUI::ctx->setup) {
						GUI::Controls::Slider(XorStr("Aspect ratio value"), &g_Vars.esp.aspect_ratio_value, 0.02f, 5.f, XorStr("%.2f"), 0.01f);
					}

					GUI::Group::EndGroup();
				}

				GUI::Group::BeginGroup(XorStr("Effects"), Vector2D(50, 50));
				{
					std::vector<MultiItem_t> world_adj = {
						{ XorStr("Nightmode"), &g_Vars.esp.night_mode },
						{ XorStr("Fullbright"), &g_Vars.esp.fullbright },
						{ XorStr("Skybox color"), &g_Vars.esp.skybox },
					};

					GUI::Controls::MultiDropdown(XorStr("World adjustment"), world_adj);
					if (g_Vars.esp.night_mode || GUI::ctx->setup) {
						GUI::Controls::Slider(XorStr("World brightness"), &g_Vars.esp.world_adjustement_value, 1.f, 100.0f, XorStr("%.0f%%"));
						GUI::Controls::Slider(XorStr("Prop brightness"), &g_Vars.esp.prop_adjustement_value, 1.f, 100.0f, XorStr("%.0f%%"));
						GUI::Controls::Slider(XorStr("Prop transparency"), &g_Vars.esp.transparent_props, 0.f, 100.0f, XorStr("%.0f%%"));
					}

					GUI::Controls::Dropdown(XorStr("Skybox changer"), {
						XorStr("Default"),
						XorStr("cs_baggage_skybox_"),
						XorStr("cs_tibet"),
						XorStr("embassy"),
						XorStr("italy"),
						XorStr("jungle"),
						XorStr("nukeblank"),
						XorStr("office"),
						XorStr("sky_csgo_cloudy01"),
						XorStr("sky_csgo_night02"),
						XorStr("sky_csgo_night02b"),
						XorStr("sky_dust"),
						XorStr("sky_venice"),
						XorStr("vertigo"),
						XorStr("vietnam"),
						XorStr("sky_descent")
						}, &g_Vars.esp.sky_changer);

					if (g_Vars.esp.skybox || GUI::ctx->setup)
						GUI::Controls::ColorPicker(XorStr("Skybox color#color"), &g_Vars.esp.skybox_modulation);

					std::vector<MultiItem_t> removals = {
						{ XorStr("Smoke effects"), &g_Vars.esp.remove_smoke },
						{ XorStr("Flashbang effects"), &g_Vars.esp.remove_flash },
						{ XorStr("Scope effects"), &g_Vars.esp.remove_scope },
						{ XorStr("Scope zoom"), &g_Vars.esp.remove_scope_zoom },
						{ XorStr("Scope blur"), &g_Vars.esp.remove_scope_blur },
						{ XorStr("Recoil shake"), &g_Vars.esp.remove_recoil_shake },
						{ XorStr("Recoil punch"), &g_Vars.esp.remove_recoil_punch },
						{ XorStr("View bob"), &g_Vars.esp.remove_bob },
					};

					GUI::Controls::MultiDropdown(XorStr("Removals"), removals);

					if (g_Vars.esp.remove_scope || GUI::ctx->setup) {
						GUI::Controls::Dropdown(XorStr("Scope effect type"), { XorStr("Static") }, &g_Vars.esp.remove_scope_type);
					}

					GUI::Controls::Checkbox(XorStr("Thirdperson"), &g_Vars.misc.third_person);
					GUI::Controls::Hotkey(XorStr("Thirdperson key"), &g_Vars.misc.third_person_bind);
					if (g_Vars.misc.third_person || GUI::ctx->setup) {
						GUI::Controls::Checkbox(XorStr("Disable on grenade"), &g_Vars.misc.third_person_on_grenade);
						GUI::Controls::Slider(XorStr("Distance"), &g_Vars.misc.third_person_dist, 0, 250, XorStr("%.0f"));
					}

					if (GUI::Controls::Checkbox(XorStr("Bullet tracers"), &g_Vars.esp.beam_enabled) || GUI::ctx->setup) {
						GUI::Controls::Dropdown(XorStr("Bullet tracers type"), { XorStr("Line"), XorStr("Beam") }, &g_Vars.esp.beam_type);
					}

					GUI::Controls::Checkbox(XorStr("Ambient lightning"), &g_Vars.esp.ambient_ligtning);
					GUI::Controls::ColorPicker(XorStr("Ambient lightning color"), &g_Vars.esp.ambient_ligtning_color);


					GUI::Controls::Checkbox(XorStr("Skip occlusion"), &g_Vars.esp.skip_occulusion);
					GUI::Controls::Checkbox(XorStr("Remove sleeve rendering"), &g_Vars.esp.remove_sleeves);

					GUI::Group::EndGroup();
				}

			}

			static bool on_cfg_load_gloves, on_cfg_load_knives;
			if (GUI::Form::BeginTab(4, XorStr("D")) || GUI::ctx->setup) {
				GUI::Group::BeginGroup(XorStr("General"), Vector2D(50, 50));
				{
					GUI::Controls::Checkbox(XorStr("Auto jump"), &g_Vars.misc.autojump);
					GUI::Controls::Checkbox(XorStr("Auto strafe"), &g_Vars.misc.autostrafer);
					GUI::Controls::Checkbox(XorStr("Move exploit"), &g_Vars.misc.move_exploit);
					GUI::Controls::Hotkey(XorStr("Move exploit key"), &g_Vars.misc.move_exploit_key);

					GUI::Controls::Checkbox(XorStr("Auto peek"), &g_Vars.misc.autopeek);
					if (g_Vars.misc.autopeek || GUI::ctx->setup) {
						GUI::Controls::Hotkey(XorStr("Auto peek key##key"), &g_Vars.misc.autopeek_bind);
						GUI::Controls::Checkbox(XorStr("Auto peek visualise"), &g_Vars.misc.autopeek_visualise);
						GUI::Controls::ColorPicker(XorStr("Auto peek color ##Auto"), &g_Vars.misc.autopeek_color);
					}

					GUI::Controls::Checkbox(XorStr("Instant stop in air"), &g_Vars.misc.instant_stop);
					//GUI::Controls::Hotkey( XorStr( "Instant stop in air key" ), &g_Vars.misc.instant_stop_key );

					GUI::Controls::Checkbox(XorStr("Directional strafe"), &g_Vars.misc.autostrafer_wasd);
					GUI::Controls::Checkbox(XorStr("Edge jump"), &g_Vars.misc.edgejump);
					GUI::Controls::Hotkey(XorStr("Edge jump key"), &g_Vars.misc.edgejump_bind);
					GUI::Controls::Checkbox(XorStr("Money revealer"), &g_Vars.misc.money_revealer);
					GUI::Controls::Checkbox(XorStr("Unlock inventory"), &g_Vars.misc.unlock_inventory);
					GUI::Controls::Checkbox(XorStr("Auto accept"), &g_Vars.misc.auto_accept);
					GUI::Controls::Checkbox(XorStr("Fast stop"), &g_Vars.misc.quickstop);
					GUI::Controls::Checkbox(XorStr("Accurate walk"), &g_Vars.misc.accurate_walk);
					GUI::Controls::Checkbox(XorStr("Slide walk"), &g_Vars.misc.slide_walk);
					GUI::Controls::Checkbox(XorStr("Infinite stamina"), &g_Vars.misc.fastduck);

					std::vector<std::string> first_weapon_str = {
						XorStr("None"),
						XorStr("SCAR-20 / G3SG1"),
						XorStr("SSG-08"),
						XorStr("AWP"),
					};

					std::vector<std::string> second_weapon_str = {
						XorStr("None"),
						XorStr("Dualies"),
						XorStr("Desert Eagle / R8 Revolver"),
					};

					std::vector<MultiItem_t> other_weapon_conditions = {
						{ XorStr("Armor"), &g_Vars.misc.autobuy_armor },
						{ XorStr("Flashbang"), &g_Vars.misc.autobuy_flashbang },
						{ XorStr("HE Grenade"), &g_Vars.misc.autobuy_hegrenade },
						{ XorStr("Molotov"), &g_Vars.misc.autobuy_molotovgrenade },
						{ XorStr("Smoke"), &g_Vars.misc.autobuy_smokegreanade },
						{ XorStr("Decoy"), &g_Vars.misc.autobuy_decoy },
						{ XorStr("Taser"), &g_Vars.misc.autobuy_zeus },
						{ XorStr("Defuse kit"), &g_Vars.misc.autobuy_defusekit },
					};

					GUI::Controls::Checkbox(XorStr("Buy bot"), &g_Vars.misc.autobuy_enabled);
					if (g_Vars.misc.autobuy_enabled || GUI::ctx->setup) {
						GUI::Controls::Dropdown(XorStr("Primary weapon"), first_weapon_str, &g_Vars.misc.autobuy_first_weapon);
						GUI::Controls::Dropdown(XorStr("Secondary Weapon"), second_weapon_str, &g_Vars.misc.autobuy_second_weapon);
						GUI::Controls::MultiDropdown(XorStr("Utility"), other_weapon_conditions);
					}

					g_Vars.misc.chat_spammer = false;

					GUI::Group::EndGroup();
				}

				GUI::Group::BeginGroup(XorStr("Cheat settings"), Vector2D(50, 50));
				{
					GUI::Controls::Checkbox(XorStr("Watermark"), &g_Vars.misc.watermark);
					GUI::Controls::Label(XorStr("Menu key"));
					GUI::Controls::Hotkey(XorStr("Menu key#MenuKey"), &g_Vars.menu.key, false);

					GUI::Controls::Label(XorStr("Menu accent color"));
					GUI::Controls::ColorPicker(XorStr("Menu accent color#MenuColor"), &g_Vars.menu.ascent);

					GUI::Controls::Checkbox(XorStr("Override log color"), &g_Vars.misc.print_color);

					if (g_Vars.misc.print_color)
						GUI::Controls::ColorPicker(XorStr("Log accent color#LogColor"), &g_Vars.misc.log_color);

					std::vector<MultiItem_t> m_indicators = {
					{ XorStr("Exploits"), &g_Vars.esp.indicator_exploits },
					{ XorStr("Damage override"), &g_Vars.esp.indicator_dmg },
					{ XorStr("Prefer baim"), &g_Vars.esp.indicator_baim },
					{ XorStr("LBY"), &g_Vars.esp.indicator_lby },
					{ XorStr("LC"), &g_Vars.esp.indicator_breaklc },
					};

					GUI::Controls::MultiDropdown(XorStr("Indicators"), m_indicators);

					GUI::Controls::Checkbox(XorStr("Clan-tag"), &g_Vars.misc.clantag_changer);

					std::vector<MultiItem_t> notifications = {
						{ XorStr("Bomb"), &g_Vars.esp.event_bomb },
					{ XorStr("Damage dealt"), &g_Vars.esp.event_dmg },
					{ XorStr("Damage taken"), &g_Vars.esp.event_harm },
					{ XorStr("Purchases"), &g_Vars.esp.event_buy },
					{ XorStr("Misses"), &g_Vars.esp.event_resolver },
					};
					GUI::Controls::MultiDropdown(XorStr("Notifications"), notifications);

					GUI::Group::EndGroup();
				}

				GUI::Group::BeginGroup(XorStr("Configs"), Vector2D(50, 100));
				{
					static int selected_cfg;
					static std::vector<std::string> cfg_list;
					static bool initialise_configs = true;
					bool reinit = false;
					if (initialise_configs || (GetTickCount() % 1000) == 0) {
						cfg_list = ConfigManager::GetConfigs();
						initialise_configs = false;
						reinit = true;
					}

					static std::string config_name;
					GUI::Controls::Textbox(XorStr("Config name"), &config_name, 26);
					GUI::Controls::Listbox(XorStr("Config selection"),
						(cfg_list.empty() ? std::vector<std::string>{XorStr("No configs")} : cfg_list), &selected_cfg, false, 6);

					if (reinit) {
						if (selected_cfg >= cfg_list.size())
							selected_cfg = cfg_list.size() - 1;

						if (selected_cfg < 0)
							selected_cfg = 0;
					}

					if (!cfg_list.empty()) {
						static bool confirm_save = false;
						GUI::Controls::Button(!confirm_save ? XorStr("Save config#save") : XorStr("Are you sure?#save"), [&]() {
							if (selected_cfg <= cfg_list.size() && selected_cfg >= 0) {
								if (!confirm_save) {
									confirm_save = true;
									return;
								}

								if (confirm_save) {
									ConfigManager::SaveConfig(cfg_list.at(selected_cfg));

									confirm_save = false;

									GUI::ctx->SliderInfo.LastChangeTime.clear();
									GUI::ctx->SliderInfo.PreviewAnimation.clear();
									GUI::ctx->SliderInfo.PreviousAmount.clear();
									GUI::ctx->SliderInfo.ShouldChangeValue.clear();
									GUI::ctx->SliderInfo.ValueTimer.clear();
									GUI::ctx->SliderInfo.ValueAnimation.clear();
								}
							}
							}, true);

						GUI::Controls::Button(XorStr("Load config"), [&]() {
							if (selected_cfg <= cfg_list.size() && selected_cfg >= 0) {
								ConfigManager::ResetConfig();

								ConfigManager::LoadConfig(cfg_list.at(selected_cfg));
								g_Vars.m_global_skin_changer.m_update_skins = true;
								g_Vars.m_global_skin_changer.m_update_gloves = true;

								GUI::ctx->SliderInfo.LastChangeTime.clear();
								GUI::ctx->SliderInfo.PreviewAnimation.clear();
								GUI::ctx->SliderInfo.PreviousAmount.clear();
								GUI::ctx->SliderInfo.ShouldChangeValue.clear();
								GUI::ctx->SliderInfo.ValueTimer.clear();
								GUI::ctx->SliderInfo.ValueAnimation.clear();

								on_cfg_load_knives = on_cfg_load_gloves = true;

								//g_Vars.menu.ascent = FloatColor( 0.51764705882, 0.45882352941, 0.90196078431, 1.0f );

								/*g_Vars.globals.m_hitsounds.clear( );

								std::string dir = GetDocumentsDirectory( ).append( XorStr( "\\json\\" ) );
								for( auto& file_path : std::filesystem::directory_iterator( dir ) ) {
									if( !file_path.path( ).string( ).empty( ) ) {
										if( file_path.path( ).string( ).find( ".wav" ) != std::string::npos ) {
											g_Vars.globals.m_hitsounds.emplace_back( file_path.path( ).string( ).erase( 0, dir.length( ) ) );
										}
									}
								}*/
							}
							});

						static bool confirm_delete = false;
						GUI::Controls::Button(!confirm_delete ? XorStr("Delete config#delete") : XorStr("Are you sure?#delete"), [&]() {
							if (selected_cfg <= cfg_list.size() && selected_cfg >= 0) {
								if (!confirm_delete) {
									confirm_delete = true;
									return;
								}

								if (confirm_delete) {
									ConfigManager::RemoveConfig(cfg_list.at(selected_cfg));
									cfg_list = ConfigManager::GetConfigs();
									confirm_delete = false;
								}
							}
							}, true);
					}

					GUI::Controls::Button(XorStr("Create config"), [&]() {
						if (config_name.empty())
							return;

						ConfigManager::CreateConfig(config_name);
						cfg_list = ConfigManager::GetConfigs();
						});

					GUI::Controls::Button(XorStr("Open config directory"), [&]() {
						ConfigManager::OpenConfigFolder();
						});

					GUI::Controls::Button(XorStr("Reset config"), [&]() {
						ConfigManager::ResetConfig();
						});

					GUI::Group::EndGroup();
				}
			}
			/*
			if (GUI::Form::BeginTab(5, XorStr("E"))) {
				static int weapon_id;
				GUI::Group::BeginGroup(XorStr("General"), Vector2D(50, 100)); {
					GUI::Controls::Checkbox(XorStr("Enabled##Skins"), &g_Vars.m_global_skin_changer.m_active);

					static std::vector<std::string> weapons;
					for (int i = 0; i < weapon_skins.size(); ++i) {
						auto whatevertheFUCK = weapon_skins[i];
						std::string ha{ whatevertheFUCK.display_name };

						// get rid of weapon_
						if (ha[0] == 'w' && ha[1] == 'e' && ha[6] == '_')
							ha.erase(0, 7);

						// get rid of knife_				
						if (ha[0] == 'k' && ha[1] == 'n' && ha[5] == '_')
							ha.erase(0, 6);

						weapons.push_back(ha.data());
					}
					if (!weapons.empty()) {
						GUI::Controls::Listbox(XorStr("Weapon skin"), weapons, &weapon_id, true, 13);
					}

					weapons.clear();

					if (GUI::Controls::Checkbox(XorStr("Override knife model##knife"), &g_Vars.m_global_skin_changer.m_knife_changer)) {
						if (k_knife_names.at(g_Vars.m_global_skin_changer.m_knife_vector_idx).definition_index != g_Vars.m_global_skin_changer.m_knife_idx) {
							auto it = std::find_if(k_knife_names.begin(), k_knife_names.end(), [&](const WeaponName_t& a) {
								return a.definition_index == g_Vars.m_global_skin_changer.m_knife_idx;
								});

							if (on_cfg_load_knives) {
								if (it != k_knife_names.end())
									g_Vars.m_global_skin_changer.m_knife_vector_idx = std::distance(k_knife_names.begin(), it);

								on_cfg_load_knives = false;
							}
						}

						std::vector<std::string> knifes;
						static bool init_knife_names = false;
						for (int i = 0; i < k_knife_names.size(); ++i) {
							auto whatevertheFUCK = k_knife_names[i];
							knifes.push_back(whatevertheFUCK.name);
						}

						if (!knifes.empty()) {
							GUI::Controls::Dropdown(XorStr("Knife models"), knifes, &g_Vars.m_global_skin_changer.m_knife_vector_idx);
							g_Vars.m_global_skin_changer.m_knife_idx = k_knife_names[g_Vars.m_global_skin_changer.m_knife_vector_idx].definition_index;
						}

						knifes.clear();
					}

					if (GUI::Controls::Checkbox(XorStr("Override glove model##glove"), &g_Vars.m_global_skin_changer.m_glove_changer)) {

						if (k_glove_names.at(g_Vars.m_global_skin_changer.m_gloves_vector_idx).definition_index != g_Vars.m_global_skin_changer.m_gloves_idx) {
							auto it = std::find_if(k_glove_names.begin(), k_glove_names.end(), [&](const WeaponName_t& a) {
								return a.definition_index == g_Vars.m_global_skin_changer.m_gloves_idx;
								});

							if (on_cfg_load_gloves) {
								if (it != k_glove_names.end())
									g_Vars.m_global_skin_changer.m_gloves_vector_idx = std::distance(k_glove_names.begin(), it);
								on_cfg_load_gloves = false;
							}
						}

						static std::vector<std::string> gloves;
						for (int i = 0; i < k_glove_names.size(); ++i) {
							auto whatevertheFUCK = k_glove_names[i];
							gloves.push_back(whatevertheFUCK.name);
						}

						static int bruh = g_Vars.m_global_skin_changer.m_gloves_vector_idx;
						if (!gloves.empty()) {
							GUI::Controls::Dropdown(XorStr("Glove model"), gloves, &g_Vars.m_global_skin_changer.m_gloves_vector_idx);
							g_Vars.m_global_skin_changer.m_gloves_idx = k_glove_names[g_Vars.m_global_skin_changer.m_gloves_vector_idx].definition_index;
						}

						if (bruh != g_Vars.m_global_skin_changer.m_gloves_vector_idx) {
							g_Vars.m_global_skin_changer.m_update_skins = true;
							g_Vars.m_global_skin_changer.m_update_gloves = true;

							bruh = g_Vars.m_global_skin_changer.m_gloves_vector_idx;
						}

						gloves.clear();
					}

					GUI::Group::EndGroup();
				}

				GUI::Group::BeginGroup(XorStr("Weapon options"), Vector2D(50, 100)); {
					auto& current_weapon = weapon_skins[weapon_id];
					auto idx = current_weapon.id;

					auto& skin_data = g_Vars.m_skin_changer;
					CVariables::skin_changer_data* skin = nullptr;
					for (size_t i = 0u; i < skin_data.Size(); ++i) {
						skin = skin_data[i];
						if (skin->m_definition_index == idx) {
							break;
						}
					}

					if (skin) {
						GUI::Controls::Checkbox(XorStr("Filter paint kits"), &skin->m_filter_paint_kits);

						if (skin->m_filter_paint_kits) {
							auto& kit = current_weapon.m_kits[skin->m_paint_kit_index];
							if (kit.id != skin->m_paint_kit) {
								auto it = std::find_if(current_weapon.m_kits.begin(), current_weapon.m_kits.end(), [skin](paint_kit& a) {
									return a.id == skin->m_paint_kit;
									});

								if (it != current_weapon.m_kits.end())
									skin->m_paint_kit_index = std::distance(current_weapon.m_kits.begin(), it);
							}
						}

						static int bruh1 = skin->m_paint_kit_index;
						static int bruh2 = skin->m_paint_kit_no_filter;

						if (skin->m_filter_paint_kits) {
							static std::vector<std::string> paint_kits;

							for (int i = 0; i < current_weapon.m_kits.size(); ++i) {
								auto whatevertheFUCK = current_weapon.m_kits[i];
								paint_kits.push_back(whatevertheFUCK.name.data());
							}

							if (!paint_kits.empty()) {
								GUI::Controls::Listbox(XorStr("Paint kits"), paint_kits, &skin->m_paint_kit_index, true, 13);
							}

							paint_kits.clear();
						}
						else {
							if (!g_Vars.globals.m_vecPaintKits.empty()) {
								GUI::Controls::Listbox(XorStr("Paint kits"), g_Vars.globals.m_vecPaintKits, &skin->m_paint_kit_no_filter, true, 13);
							}
						}

						if ((bruh1 != skin->m_paint_kit_index) || (bruh2 != skin->m_paint_kit_no_filter)) {
							g_Vars.m_global_skin_changer.m_update_skins = true;
							if (current_weapon.glove)
								g_Vars.m_global_skin_changer.m_update_gloves = true;

							bruh1 = skin->m_paint_kit_index;
							bruh2 = skin->m_paint_kit_no_filter;
						}

						skin->m_paint_kit = skin->m_filter_paint_kits ? current_weapon.m_kits[skin->m_paint_kit_index].id : skin->m_paint_kit_no_filter;

						skin->m_enabled = true;

						GUI::Controls::Slider(XorStr("Wear"), &skin->m_wear, 0.00000001f, 1.00f, XorStr("%.5f%%"));
						GUI::Controls::Slider(XorStr("Seed"), &skin->m_seed, 1, 1000, XorStr("%.0f"));
					}

					GUI::Group::EndGroup();
				}
			}*/

			GUI::Form::EndWindow();
		}
	}
	void Draw() {
		DrawMenu();
	}
}
