

#include "../Hooked.hpp"
#include "../../Features/Game/Prediction.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/sdk.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../Features/Visuals/EventLogger.hpp"
#include "../../Features/Visuals/GrenadePrediction.hpp"
#include "../../Features/Visuals/ESP.hpp"
#include "../../Renderer/Render.hpp"
#include "../../Features/Visuals/ExtendedEsp.hpp"
#include "../../Features/Miscellaneous/Movement.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../SDK/Classes/Exploits.hpp"
#include "../../Features/Miscellaneous/Miscellaneous.hpp"
#include "../../Features/Game/SetupBones.hpp"
#include "../../Features/Rage/LagCompensation.hpp"
#include "../../Utils/Threading/threading.h"
#include "../../Features/Rage/AnimationSystem.hpp"
#include "../../Features/Rage/ShotInformation.hpp"
#include "../../Features/Rage/Resolver.hpp"
#include "../../Features/Miscellaneous/BulletBeamTracer.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"
#include "../../Features/Visuals/CChams.hpp"
#include "../../Features/Miscellaneous/WeatherController.hpp"

#ifdef DEV
//#define SERVER_HITBOXES
#endif

#ifdef DEBUG_SHOOTING
extern bool debugShoot;
#endif

float WorldExposure = 0.0f;
float ModelAmbient = 0.0f;
float BloomScale = 0.0f;

extern bool MoveJitter;
extern Vector JitterOrigin;

float RenderViewOffsetZ = 0.0f;

bool RestoreData = false;
Vector org_backup, vel_backup;

#ifdef SERVER_HITBOXES
void draw_server_hitboxes(int index) {
	if (g_Vars.globals.m_iServerType != 8)
		return;

	auto get_player_by_index = [](int index) -> C_CSPlayer* { //i dont need this shit func for anything else so it can be lambda
		typedef C_CSPlayer* (__fastcall* player_by_index)(int);
		static auto player_index = reinterpret_cast<player_by_index>(Memory::Scan(XorStr("server.dll"), XorStr("85 C9 7E 2A A1")));

		if (!player_index)
			return false;

		return player_index(index);
	};

	static auto fn = Memory::Scan(XorStr("server.dll"), XorStr("55 8B EC 81 EC ? ? ? ? 53 56 8B 35 ? ? ? ? 8B D9 57 8B CE"));
	auto duration = -1.f;
	PVOID entity = nullptr;

	entity = get_player_by_index(index);

	if (!entity)
		return;

	__asm {
		pushad
		movss xmm1, duration
		push 0 // 0 - colored, 1 - blue
		mov ecx, entity
		call fn
		popad
	}
}
#endif

namespace Hooked
{
	enum PostProcessParameterNames_t
	{
		PPPN_FADE_TIME = 0,
		PPPN_LOCAL_CONTRAST_STRENGTH,
		PPPN_LOCAL_CONTRAST_EDGE_STRENGTH,
		PPPN_VIGNETTE_START,
		PPPN_VIGNETTE_END,
		PPPN_VIGNETTE_BLUR_STRENGTH,
		PPPN_FADE_TO_BLACK_STRENGTH,
		PPPN_DEPTH_BLUR_FOCAL_DISTANCE,
		PPPN_DEPTH_BLUR_STRENGTH,
		PPPN_SCREEN_BLUR_STRENGTH,
		PPPN_FILM_GRAIN_STRENGTH,

		POST_PROCESS_PARAMETER_COUNT
	};

	struct PostProcessParameters_t
	{
		PostProcessParameters_t()
		{
			memset(m_flParameters, 0, sizeof(m_flParameters));
			m_flParameters[PPPN_VIGNETTE_START] = 0.8f;
			m_flParameters[PPPN_VIGNETTE_END] = 1.1f;
		}

		float m_flParameters[POST_PROCESS_PARAMETER_COUNT];
	};

	void __fastcall FrameStageNotify(void* ecx, void* edx, ClientFrameStage_t stage) {
		g_Vars.globals.szLastHookCalled = XorStr("9");
		auto local = C_CSPlayer::GetLocalPlayer();

		static auto mat_postprocess_enable = Interfaces::m_pCvar->FindVar(XorStr("mat_postprocess_enable"));

		// paranoic af
		g_Vars.globals.HackIsReady = local
			&& Interfaces::m_pEngine->IsInGame()
			&& Interfaces::m_pClientState->m_nDeltaTick() > 0
			&& local->m_flSpawnTime() > 0.f;

		// hack.
		if (!g_Vars.globals.HackIsReady) {
			Interfaces::IChams::Get()->OnPostScreenEffects();
			//g_TickbaseController.bExploiting = false;

			Engine::WeatherController::Get()->ResetWeather();
			g_Vars.globals.bCreatedRain = false;
		}

		if (g_Vars.esp.remove_post_proccesing) {
			static auto PostProcessParameters = *reinterpret_cast<PostProcessParameters_t**>((uintptr_t)Memory::Scan(("client.dll"), ("0F 11 05 ? ? ? ? 0F 10 87")) + 3);
			static float backupblur = PostProcessParameters->m_flParameters[PPPN_VIGNETTE_BLUR_STRENGTH];

			mat_postprocess_enable->SetValue(XorStr("0"));

			float blur = g_Vars.esp.remove_post_proccesing ? 0.f : backupblur;
			if (PostProcessParameters->m_flParameters[PPPN_VIGNETTE_BLUR_STRENGTH] != blur)
				PostProcessParameters->m_flParameters[PPPN_VIGNETTE_BLUR_STRENGTH] = blur;
		}
		else
			mat_postprocess_enable->SetValue(XorStr("1"));

		// cache random values cuz valve random system cause performance issues
		if (!g_Vars.globals.RandomInit) {
			std::vector<std::pair<int, float>> biggsetPi1;
			int low_seeds_count = 0;
			int high_seeds_count = 0;
			int seeds_count = 0;
			int add_seeds_count = 0;
			bool low_seeds_created = false;
			bool high_seeds_created = false;

			while (seeds_count < 200 + add_seeds_count) {
				seeds_count++;
				bool add_seed = false;

				RandomSeed(seeds_count + 1);

				float pi1 = RandomFloat(0.0f, 6.2831855f);
				float seed = RandomFloat(0.0f, 1.0f);
				float pi2 = RandomFloat(0.0f, 6.2831855f);

				if (low_seeds_count >= 50)
					low_seeds_created = true;

				if (high_seeds_count >= 100)
					high_seeds_created = true;

				if (!low_seeds_created) {
					if (seed < 0.5f && low_seeds_count < 50) {
						add_seed = true;
						low_seeds_count++;
					}
					else
						add_seeds_count++;
				}
				else if (!high_seeds_created) {
					if (seed > 0.5f && high_seeds_count < 100) {
						add_seed = true;
						high_seeds_count++;
					}
					else
						add_seeds_count++;
				}

				if (add_seed) {
					g_Vars.globals.SpreadRandom[seeds_count - add_seeds_count].flRand1 = seed;
					g_Vars.globals.SpreadRandom[seeds_count - add_seeds_count].flRandPi1 = pi1;
					g_Vars.globals.SpreadRandom[seeds_count - add_seeds_count].flRand2 = seed;
					g_Vars.globals.SpreadRandom[seeds_count - add_seeds_count].flRandPi2 = pi2;
				}
			}

			g_Vars.globals.RandomInit = true;
		}

		if (stage == FRAME_RENDER_START && Interfaces::m_pEngine->IsConnected() && local) {

			if (g_Vars.r_rainalpha->fnChangeCallback.m_Size != 0)
				g_Vars.r_rainalpha->fnChangeCallback.m_Size = 0;

			if (g_Vars.r_rainalpha->GetFloat() != g_Vars.esp.weather_alpha * 0.01f) {
				g_Vars.r_rainalpha->SetValueFloat(g_Vars.esp.weather_alpha * 0.01f);
			}

			for (int i = 1; i <= Interfaces::m_pGlobalVars->maxClients; ++i) {
				auto player = C_CSPlayer::GetPlayerByIndex(i);

				if (!local)
					continue;

				if (!player || player->IsDead())
					continue;

				if (player == local || player->IsTeammate(local))
					continue;

				player_info_t player_info;
				if (!Interfaces::m_pEngine->GetPlayerInfo(i, &player_info))
					continue;

				auto lagData = Engine::LagCompensation::Get()->GetLagData(player->m_entIndex);
				if (!lagData.IsValid() || lagData->m_History.empty())
					continue;

				player->SetAbsOrigin(player->m_vecOrigin());
				player->m_bSpotted() = g_Vars.misc.ingame_radar;
			}
		}

		if (stage == FRAME_RENDER_START) {
			if (g_Vars.globals.HackIsReady) {
				static bool bShouldCall = false;
				if (bShouldCall && !g_Vars.esp.weather) {
					Engine::WeatherController::Get()->ResetWeather();
					bShouldCall = false;
				}

				if (!bShouldCall && g_Vars.esp.weather) {
					g_Vars.globals.bCreatedRain = false;
					bShouldCall = true;
				}

				if (bShouldCall) {
					Engine::WeatherController::Get()->UpdateWeather();
				}
			}

			if (g_Vars.esp.remove_smoke) {
				static auto smoke_count = *reinterpret_cast<uintptr_t*>(Engine::Displacement.DT_SmokeGrenadeProjectile.m_nSmokeCount);
				if (smoke_count)
					*reinterpret_cast<int*>(smoke_count) = 0;
			}

			static bool bReset = false;
			if (g_Vars.esp.ambient_ligtning) {
				bReset = false;
				if (g_Vars.mat_ambient_light_r->GetFloat() != g_Vars.esp.ambient_ligtning_color.r)
					g_Vars.mat_ambient_light_r->SetValueFloat(g_Vars.esp.ambient_ligtning_color.r);

				if (g_Vars.mat_ambient_light_g->GetFloat() != g_Vars.esp.ambient_ligtning_color.g)
					g_Vars.mat_ambient_light_g->SetValueFloat(g_Vars.esp.ambient_ligtning_color.g);

				if (g_Vars.mat_ambient_light_b->GetFloat() != g_Vars.esp.ambient_ligtning_color.b)
					g_Vars.mat_ambient_light_b->SetValueFloat(g_Vars.esp.ambient_ligtning_color.b);
			}
			else {
				if (!bReset) {
					g_Vars.mat_ambient_light_r->SetValueFloat(0.f);
					g_Vars.mat_ambient_light_g->SetValueFloat(0.f);
					g_Vars.mat_ambient_light_b->SetValueFloat(0.f);
					bReset = true;
				}
			}
		}

		static QAngle qAimPunch, qViewPunch;
		static QAngle* qpAimPunch = nullptr, * qpViewPunch = nullptr;
		if (g_Vars.globals.HackIsReady) {
			static float networkedCycle = 0.f;
			static float animationTime = 0.f;

			RenderViewOffsetZ = local->m_vecViewOffset().z;

			auto viewModel = local->m_hViewModel();

			// m_flAnimTime : 0x260
			if (viewModel) {
				// onetap.su
				if (stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START && g_Vars.globals.UnknownCycleFix && *(float*)(uintptr_t(viewModel.Get()) + Engine::Displacement.DT_BaseAnimating.m_flCycle) == 0.0f) {
					*(float*)(uintptr_t(viewModel.Get()) + Engine::Displacement.DT_BaseAnimating.m_flCycle) = networkedCycle;
					*(float*)(uintptr_t(viewModel.Get()) + Engine::Displacement.DT_BaseEntity.m_flAnimTime) = animationTime;
					g_Vars.globals.UnknownCycleFix = false;
				}

				networkedCycle = *(float*)(uintptr_t(viewModel.Get()) + Engine::Displacement.DT_BaseAnimating.m_flCycle);
				animationTime = *(float*)(uintptr_t(viewModel.Get()) + Engine::Displacement.DT_BaseEntity.m_flAnimTime);
			}

			if (stage == FRAME_RENDER_START && Interfaces::m_pEngine->IsConnected()) {


#ifdef SERVER_HITBOXES
				if (Interfaces::m_pInput->CAM_IsThirdPerson())
					draw_server_hitboxes(local->EntIndex());
#endif

				auto aim_punch = local->m_aimPunchAngle() * g_Vars.weapon_recoil_scale->GetFloat() * g_Vars.view_recoil_tracking->GetFloat();
				if (g_Vars.esp.remove_recoil_shake)
					local->pl().v_angle -= local->m_viewPunchAngle() /*+ aim_punch*/;

				if (g_Vars.esp.remove_recoil_punch)
					local->pl().v_angle -= aim_punch;

				local->pl().v_angle.Normalize();

				if (g_Vars.esp.remove_flash)
					local->m_flFlashDuration() = 0.0f;


				if (g_Vars.misc.third_person && g_Vars.misc.third_person_bind.enabled) {
					Interfaces::m_pPrediction->SetLocalViewAngles(g_Vars.globals.RegularAngles);
				}
			}


			// fix netvar compression
			auto& prediction = Engine::Prediction::Instance();
			if (Interfaces::m_pEngine->IsInGame())
				prediction.OnFrameStageNotify(stage);
		}
		else {
			g_Vars.globals.RenderIsReady = false;
		}

		if (stage == FRAME_RENDER_END) {
			static bool IsConnected = false;

			if (Interfaces::m_pEngine->IsConnected() && !IsConnected) {
				if (!_stricmp(Interfaces::m_pEngine->GetNetChannelInfo()->GetAddress(), XorStr("loopback")))
					g_Vars.globals.server_adress = XorStr("Local server");
				else
					g_Vars.globals.server_adress = Interfaces::m_pEngine->GetNetChannelInfo()->GetAddress();

				IsConnected = true;
			}

			if (!Interfaces::m_pEngine->IsConnected())
				IsConnected = false;

			if (Interfaces::m_pEngine->IsConnected()) {
				static auto g_GameRules = *(uintptr_t**)(Engine::Displacement.Data.m_GameRules);
				if (g_GameRules && *(bool*)(*(uintptr_t*)g_GameRules + 0x75)) {
					g_Vars.globals.server_adress = XorStr("Valve server");

					if (g_Vars.game_type->GetInt() == 0) { // classic
						g_Vars.globals.m_iServerType = 1;
						if (g_Vars.game_mode->GetInt() == 0) {
							g_Vars.globals.m_iGameMode = 1; // casual
						}
						else if (g_Vars.game_mode->GetInt() == 1) {
							g_Vars.globals.m_iGameMode = 2; // competitive
						}
						else if (g_Vars.game_mode->GetInt() == 2) {
							g_Vars.globals.m_iGameMode = 3; // scrimcomp2v2
						}
						else if (g_Vars.game_mode->GetInt() == 3) {
							g_Vars.globals.m_iGameMode = 4; // scrimcomp5v5
						}
					}
					else if (g_Vars.game_type->GetInt() == 1) { // gungame
						g_Vars.globals.m_iServerType = 2;
						if (g_Vars.game_mode->GetInt() == 0) {
							g_Vars.globals.m_iGameMode = 1; // gungameprogressive
						}
						else if (g_Vars.game_mode->GetInt() == 1) {
							g_Vars.globals.m_iGameMode = 2; // gungametrbomb
						}
						else if (g_Vars.game_mode->GetInt() == 2) {
							g_Vars.globals.m_iGameMode = 3; // deathmatch
						}
					}
					else if (g_Vars.game_type->GetInt() == 2) { //training
						g_Vars.globals.m_iServerType = 3;
						g_Vars.globals.m_iGameMode = 0;
					}
					else if (g_Vars.game_type->GetInt() == 3) { //custom
						g_Vars.globals.m_iServerType = 4;
						g_Vars.globals.m_iGameMode = 0;
					}
					else if (g_Vars.game_type->GetInt() == 4) { //cooperative
						g_Vars.globals.m_iServerType = 5;
						g_Vars.globals.m_iGameMode = 0;
					}
					else if (g_Vars.game_type->GetInt() == 5) { //skirmish
						g_Vars.globals.m_iServerType = 6;
						g_Vars.globals.m_iGameMode = 0;
					}
					else if (g_Vars.game_type->GetInt() == 6) { //freeforall ( danger zone )
						g_Vars.globals.m_iServerType = 7;
						g_Vars.globals.m_iGameMode = 0;
					}
				}
				else {
					// onetap way
					if (!_stricmp(Interfaces::m_pEngine->GetNetChannelInfo()->GetAddress(), XorStr("loopback")))
						g_Vars.globals.m_iServerType = 8; // local server
					else
						g_Vars.globals.m_iServerType = 9; // no valve server
				}
			}
			else
				g_Vars.globals.m_iServerType = -1; // no connected

			Interfaces::Miscellaneous::Get()->Main();

			if (g_Vars.esp.beam_enabled && g_Vars.globals.HackIsReady && g_Vars.globals.RenderIsReady && g_Vars.esp.beam_type == 1)
				IBulletBeamTracer::Get()->Main();

			if (g_Vars.globals.HackIsReady)
				g_Vars.globals.RenderIsReady = true;
		}

		// m_flAnimTime : 0x260
		if (g_Vars.globals.HackIsReady) {
			static float m_flCycle = 0.0f;
			static float m_flAnimTime = 0.f;

			auto viewModel = local->m_hViewModel().Get();

			if (viewModel) {
				static auto cycle_offset = Memory::FindInDataMap(local->GetPredDescMap(), XorStr("m_flCycle"));
				static auto anim_time_offset = Memory::FindInDataMap(local->GetPredDescMap(), XorStr("m_flAnimTime"));

				// onetap.su
				if (stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START) {
					*(float*)(uintptr_t(local) + cycle_offset) = m_flCycle;
					*(float*)(uintptr_t(local) + anim_time_offset) = m_flAnimTime;
				}

				m_flCycle = *(float*)(uintptr_t(local) + cycle_offset);
				m_flAnimTime = *(float*)(uintptr_t(local) + anim_time_offset);
			}
		}

		if (g_Vars.globals.HackIsReady) {
			if (stage == FRAME_NET_UPDATE_END && Interfaces::m_pEngine->IsConnected()) {

				Engine::C_ShotInformation::Get()->Start();


				Engine::AnimationSystem::Get()->CollectData();
				Engine::AnimationSystem::Get()->Update();
				Threading::FinishQueue(true);
				Engine::LagCompensation::Get()->Update();

				// fix issues when players we are spectating scope in
				if (local && local->m_iObserverMode() == 5) {
					local->m_iFOV() = 90.f;
				}
			}
		}

		oFrameStageNotify(ecx, stage);

		if (stage == FRAME_NET_UPDATE_END) {
			Hooked::CL_FireEvents();
		}
	}

	void __fastcall View_Render(void* ecx, void* edx, vrect_t* rect) {
		oView_Render(ecx, rect);
	}
}