#include "AntiAim.hpp"
#include "../../SDK/CVariables.hpp"
#include "../Miscellaneous/Movement.hpp"
#include "../../source.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "LagCompensation.hpp"
#include "Autowall.h"
#include "../Game/SimulationContext.hpp"
#include "../../SDK/displacement.hpp"
#include "../../Renderer/Render.hpp"
#include "../Visuals/ESP.hpp"
#include <random>

namespace Interfaces
{
	class C_AntiAimbot : public AntiAimbot {
	public:
		void UpdateJitter();
		void Main(bool* bSendPacket, bool* bFinalPacket, Encrypted_t<CUserCmd> cmd, bool ragebot) override;
	private:
		virtual float GetAntiAimX(Encrypted_t<CVariables::ANTIAIM_STATE> settings);
		virtual float GetAntiAimY(Encrypted_t<CVariables::ANTIAIM_STATE> settings, Encrypted_t<CUserCmd> cmd);

		virtual void Distort(Encrypted_t<CUserCmd> cmd);

		enum class Directions : int {
			YAW_RIGHT = -1,
			YAW_BACK,
			YAW_LEFT,
			YAW_NONE,
		};
		virtual Directions HandleDirection(Encrypted_t<CUserCmd> cmd);

		virtual bool IsEnabled(Encrypted_t<CUserCmd> cmd, Encrypted_t<CVariables::ANTIAIM_STATE> settings);

		bool m_bNegate = false;
		float m_flLowerBodyUpdateTime = 0.f;

		bool   m_jitter_update = false;
		float  m_auto;
		float  m_auto_dist;
		float  m_auto_last;
		float  m_view;
		float  m_auto_time;
	};

	bool C_AntiAimbot::IsEnabled(Encrypted_t<CUserCmd> cmd, Encrypted_t<CVariables::ANTIAIM_STATE> settings) {
		C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer();
		if (!LocalPlayer || LocalPlayer->IsDead())
			return false;

		if (g_Vars.globals.IsRoundFreeze)
			return false;

		if (!(g_Vars.antiaim.bomb_activity && g_Vars.globals.BobmActivityIndex == LocalPlayer->EntIndex()) || !g_Vars.antiaim.bomb_activity)
			if ((cmd->buttons & IN_USE) && (LocalPlayer->m_bIsDefusing()))
				return false;

		if (LocalPlayer->m_MoveType() == MOVETYPE_NOCLIP)
			return false;

		static auto g_GameRules = *(uintptr_t**)(Engine::Displacement.Data.m_GameRules);
		if (g_GameRules && *(bool*)(*(uintptr_t*)g_GameRules + 0x20) || (LocalPlayer->m_fFlags() & (1 << 6)))
			return false;

		C_WeaponCSBaseGun* Weapon = (C_WeaponCSBaseGun*)LocalPlayer->m_hActiveWeapon().Get();


		if (cmd->buttons & IN_USE)
			return false;

		if (!Weapon)
			return false;

		auto WeaponInfo = Weapon->GetCSWeaponData();
		if (!WeaponInfo.IsValid())
			return false;

		if (WeaponInfo->m_iWeaponType == WEAPONTYPE_GRENADE) {
			if (!Weapon->m_bPinPulled() || (cmd->buttons & (IN_ATTACK | IN_ATTACK2))) {
				float throwTime = Weapon->m_fThrowTime();
				if (throwTime > 0.f)
					return false;
			}
		}
		else {
			if ((WeaponInfo->m_iWeaponType == WEAPONTYPE_KNIFE && cmd->buttons & (IN_ATTACK | IN_ATTACK2)) || cmd->buttons & IN_ATTACK) {
				if (LocalPlayer->CanShoot())
					return false;
			}
		}

		if (LocalPlayer->m_MoveType() == MOVETYPE_LADDER)
			return false;

		return true;
	}

	Encrypted_t<AntiAimbot> AntiAimbot::Get() {
		static C_AntiAimbot instance;
		return &instance;
	}


	std::random_device random;
	std::mt19937 generator(random());

	void C_AntiAimbot::UpdateJitter() {

		const auto jitterSpeed = 1 + Interfaces::m_pClientState->m_nChokedCommands();
		static int lastTick = 0;
		static auto returnValue = 0.f;
		C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer();

		if (lastTick + jitterSpeed < LocalPlayer->m_nTickBase() || lastTick > LocalPlayer->m_nTickBase()) {
			lastTick = LocalPlayer->m_nTickBase();
			m_jitter_update = !m_jitter_update;
		}
	}


	void C_AntiAimbot::Main(bool* bSendPacket, bool* bFinalPacket, Encrypted_t<CUserCmd> cmd, bool ragebot) {
		C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer();

		if (!LocalPlayer || LocalPlayer->IsDead())
			return;

		auto animState = LocalPlayer->m_PlayerAnimState();
		if (!animState)
			return;

		if (!g_Vars.antiaim.enabled)
			return;

		Encrypted_t<CVariables::ANTIAIM_STATE> settings(&g_Vars.antiaim_stand);

		C_WeaponCSBaseGun* Weapon = (C_WeaponCSBaseGun*)LocalPlayer->m_hActiveWeapon().Get();

		if (!Weapon)
			return;

		auto WeaponInfo = Weapon->GetCSWeaponData();
		if (!WeaponInfo.IsValid())
			return;

		if (!IsEnabled(cmd, settings))
			return;

		if (LocalPlayer->m_MoveType() == MOVETYPE_LADDER) {
			auto eye_pos = LocalPlayer->GetEyePosition();

			CTraceFilterWorldAndPropsOnly filter;
			CGameTrace tr;
			Ray_t ray;
			float angle = 0.0f;
			while (true) {
				float cosa, sina;
				DirectX::XMScalarSinCos(&cosa, &sina, angle);

				Vector pos;
				pos.x = (cosa * 32.0f) + eye_pos.x;
				pos.y = (sina * 32.0f) + eye_pos.y;
				pos.z = eye_pos.z;

				ray.Init(eye_pos, pos,
					Vector(-1.0f, -1.0f, -4.0f),
					Vector(1.0f, 1.0f, 4.0f));
				Interfaces::m_pEngineTrace->TraceRay(ray, MASK_SOLID, &filter, &tr);
				if (tr.fraction < 1.0f)
					break;

				angle += DirectX::XM_PIDIV2;
				if (angle >= DirectX::XM_2PI) {
					return;
				}
			}

			float v23 = atan2(tr.plane.normal.x, std::fabsf(tr.plane.normal.y));
			float v24 = RAD2DEG(v23) + 90.0f;
			cmd->viewangles.pitch = 89.0f;
			if (v24 <= 180.0f) {
				if (v24 < -180.0f) {
					v24 = v24 + 360.0f;
				}
				cmd->viewangles.yaw = v24;
			}
			else {
				cmd->viewangles.yaw = v24 - 360.0f;
			}

			if (cmd->buttons & IN_BACK) {
				cmd->buttons |= IN_FORWARD;
				cmd->buttons &= ~IN_BACK;
			}
			else  if (cmd->buttons & IN_FORWARD) {
				cmd->buttons |= IN_BACK;
				cmd->buttons &= ~IN_FORWARD;
			}

			return;
		}

		bool move = LocalPlayer->m_vecVelocity().Length2D() > 0.1f; //&& !g_Vars.globals.Fakewalking;

		// save view, depending if locked or not.
		if ((g_Vars.antiaim.freestand_lock && move) || !g_Vars.antiaim.freestand_lock)
			m_view = cmd->viewangles.y;

		UpdateJitter();
		cmd->viewangles.x = GetAntiAimX(settings);
		float flYaw = GetAntiAimY(settings, cmd);

		// https://github.com/VSES/SourceEngine2007/blob/master/se2007/engine/cl_main.cpp#L1877-L1881
		if (!*bSendPacket || !*bFinalPacket) {
			cmd->viewangles.y = flYaw;
			Distort(cmd);
		}
		else {
			std::uniform_int_distribution random(-90, 90);

			static int negative = false;

			switch (settings->fake_yaw) {

				// default
			case 1:
				cmd->viewangles.y = Math::AngleNormalize(flYaw + 180 + random(generator));
				break;

				// flicker
			case 2:
				negative ? flYaw = Math::AngleNormalize(flYaw + 90 + std::fmod(Interfaces::m_pGlobalVars->curtime * 360.f, 180.f)) : cmd->viewangles.y = Math::AngleNormalize(flYaw + 90 + std::fmod(Interfaces::m_pGlobalVars->curtime * 360.f, 180.f));
				if (flYaw == 1.f)
					negative ? flYaw + std::fmod(Interfaces::m_pGlobalVars->curtime * 360.f, 0.f) : flYaw + std::fmod(Interfaces::m_pGlobalVars->curtime * 360.f, 0.f);
				negative = !negative;
				break;
			
			default:
				break;
			}
		}

		static int negative = false;
		auto bSwap = std::fabs(Interfaces::m_pGlobalVars->curtime - g_Vars.globals.m_flBodyPred) > 1.1 - (Interfaces::m_pGlobalVars->interval_per_tick * 5);
		if (!Interfaces::m_pClientState->m_nChokedCommands()
			&& Interfaces::m_pGlobalVars->curtime >= g_Vars.globals.m_flBodyPred
			&& (LocalPlayer->m_fFlags() & FL_ONGROUND) && !move) {
			// lby.
			switch (settings->yaw) {
			case 1: // twist
				cmd->viewangles.y += negative ? g_Vars.antiaim.break_lby : -g_Vars.antiaim.break_lby;
				negative = !negative;
				break;

			case 2: // static		
				cmd->viewangles.y += g_Vars.antiaim.break_lby;
				break;

			case 3: // break logic
				cmd->viewangles.y += 110.f;
				negative ? cmd->viewangles.y += 110.f : cmd->viewangles.y -= 130.f;
				negative = !negative;
				break;

			default:
				break;
			}
		}
	}

	float C_AntiAimbot::GetAntiAimX(Encrypted_t<CVariables::ANTIAIM_STATE> settings) {
		switch (settings->pitch) {
		case 1: // down
			return 89.f;
		case 2: // up 
			return -89.f;
		case 3: // zero
			return 0.f;
		default:
			break;
		}
	}

	float C_AntiAimbot::GetAntiAimY(Encrypted_t<CVariables::ANTIAIM_STATE> settings, Encrypted_t<CUserCmd> cmd) {
		auto local = C_CSPlayer::GetLocalPlayer();
		if (!local || local->IsDead())
			return FLT_MAX;

		float flViewAnlge = cmd->viewangles.y;
		float flRetValue = flViewAnlge + 180.f;
		bool bUsingManualAA = g_Vars.globals.manual_aa != -1;

		if (!g_Vars.globals.m_bGround) { // 180z
			//	flRetValue = (flViewAnlge - 180.f / 2.f);
			flRetValue -= 90;
			flRetValue += std::fmod(Interfaces::m_pGlobalVars->curtime * (6.f * 20.f), 180.f);
			return flRetValue;
		}

		// lets do our real yaw.'
		switch (settings->base_yaw) {
		case 1: // backwards.
			flRetValue = flViewAnlge + 180.f;
			break;
		case 2: // 180z
			flRetValue = (flViewAnlge - 180.f / 2.f);
			flRetValue += std::fmod(Interfaces::m_pGlobalVars->curtime * (3.5 * 20.f), 180.f);
			break;
		case 3:
			flRetValue = flViewAnlge + 180.f;
			if (settings->jitter_mode == 0)
				flRetValue += RandomFloat(-g_Vars.antiaim.random_jitter / 2, g_Vars.antiaim.random_jitter / 2);
			else
				flRetValue += m_jitter_update ? -g_Vars.antiaim.random_jitter / 2 : g_Vars.antiaim.random_jitter / 2;
			break;
		default:
			break;
		}

		if (g_Vars.antiaim.freestand) {
			const C_AntiAimbot::Directions Direction = HandleDirection(cmd);
			switch (Direction) {
			case Directions::YAW_BACK:
				switch (settings->base_yaw) {
				case 1: // backwards.
					flRetValue = flViewAnlge + 180.f;
					break;
				case 2: // 180z
					flRetValue = (flViewAnlge - 180.f / 2.f);
					flRetValue += std::fmod(Interfaces::m_pGlobalVars->curtime * (3.5 * 20.f), 180.f);
					break;
				case 3:
					flRetValue = flViewAnlge + 180.f;
					if (settings->jitter_mode == 0)
						flRetValue += RandomFloat(-g_Vars.antiaim.random_jitter / 2 * 1.15, g_Vars.antiaim.random_jitter / 2 * 1.15);
					else
						flRetValue += m_jitter_update ? -g_Vars.antiaim.random_jitter / 2 * 1.15 : g_Vars.antiaim.random_jitter / 2 * 1.15;
					break;
				default:
					break;
				}
				break;
			case Directions::YAW_LEFT:
				switch (settings->base_yaw) {
				case 1: // backwards.
					flRetValue = flViewAnlge + 90.f;
					break;
				case 2: // 180z
					flRetValue = (flViewAnlge + 90.f / 2.f);
					flRetValue += std::fmod(Interfaces::m_pGlobalVars->curtime * (3.5 * 20.f), 90.f);
					break;
				case 3:
					flRetValue = flViewAnlge + 90.f;
					if (settings->jitter_mode == 0)
						flRetValue += RandomFloat(-g_Vars.antiaim.random_jitter / 2, g_Vars.antiaim.random_jitter / 2);
					else
						flRetValue += m_jitter_update ? -g_Vars.antiaim.random_jitter / 2 : g_Vars.antiaim.random_jitter / 2;
					break;
				default:
					break;
				}
				break;
			case Directions::YAW_RIGHT:
				// right yaw.
				switch (settings->base_yaw) {
				case 1: // backwards.
					flRetValue = flViewAnlge - 90.f;
					break;
				case 2: // 180z
					flRetValue = (flViewAnlge - 90.f / 2.f);
					flRetValue += std::fmod(Interfaces::m_pGlobalVars->curtime * (3.5 * 20.f), -90.f);
					break;
				case 3:
					flRetValue = flViewAnlge - 90.f;
					if (settings->jitter_mode == 0)
						flRetValue += RandomFloat(-g_Vars.antiaim.random_jitter / 2, g_Vars.antiaim.random_jitter / 2);
					else
						flRetValue += m_jitter_update ? -g_Vars.antiaim.random_jitter / 2 : g_Vars.antiaim.random_jitter / 2;
					break;
				default:
					break;
				}
				break;
			case Directions::YAW_NONE:
				switch (settings->base_yaw) {
				case 1: // backwards.
					flRetValue = flViewAnlge + 180.f;
					break;
				case 2: // 180z
					flRetValue = (flViewAnlge - 180.f / 2.f);
					flRetValue += std::fmod(Interfaces::m_pGlobalVars->curtime * (3.5 * 20.f), 180.f);
					break;
				case 3:
					flRetValue = flViewAnlge + 180.f;
					if (settings->jitter_mode == 0)
						flRetValue += RandomFloat(-g_Vars.antiaim.random_jitter / 2 * 1.15, g_Vars.antiaim.random_jitter / 2 * 1.15);
					else
						flRetValue += m_jitter_update ? -g_Vars.antiaim.random_jitter / 2 * 1.15 : g_Vars.antiaim.random_jitter / 2 * 1.15;
					break;
				default:
					break;
				}
				break;
			}
		}

		if (settings->base_yaw > 0)
			flRetValue += g_Vars.antiaim.yaw_addition;

		if (bUsingManualAA) {
			switch (g_Vars.globals.manual_aa) {
			case 0:
				flRetValue = flViewAnlge + 90.f;
				break;
			case 1:
				flRetValue = flViewAnlge + 180.f;
				break;
			case 2:
				flRetValue = flViewAnlge - 90.f;
				break;
			}
		}


		return flRetValue;
	}

	void C_AntiAimbot::Distort(Encrypted_t<CUserCmd> cmd) {
		auto local = C_CSPlayer::GetLocalPlayer();
		if (!local || local->IsDead())
			return;

		if (!g_Vars.antiaim.distort)
			return;

		bool bDoDistort = true;

		if (g_Vars.antiaim.distort_disable_fakewalk && g_Vars.misc.slow_walk && g_Vars.misc.slow_walk_bind.enabled)
			bDoDistort = false;

		if (g_Vars.antiaim.distort_disable_air && !FL_ONGROUND)
			bDoDistort = false;

		static float flLastMoveTime = FLT_MAX;
		static float flLastMoveYaw = FLT_MAX;
		static bool bGenerate = true;
		static float flGenerated = 0.f;

		if (!(g_Vars.misc.slow_walk && g_Vars.misc.slow_walk_bind.enabled) && local->m_vecVelocity().Length() > 0.1f && g_Vars.globals.m_bGround) {
			flLastMoveTime = Interfaces::m_pGlobalVars->realtime;
			flLastMoveYaw = local->m_flLowerBodyYawTarget();

			if (g_Vars.antiaim.distort_disable_run)
				bDoDistort = false;
		}

		if (g_Vars.globals.manual_aa != -1 && !g_Vars.antiaim.distort_manual_aa)
			bDoDistort = false;

		if (flLastMoveTime == FLT_MAX)
			return;

		if (flLastMoveYaw == FLT_MAX)
			return;

		if (!bDoDistort) {
			bGenerate = true;
		}

		if (!g_Vars.antiaim.distort_spin) {
			if (bDoDistort) {

				if (true) {
					float flDistortion = std::sin((Interfaces::m_pGlobalVars->realtime * (g_Vars.antiaim.distort_speed * 2)) * 0.5f + 0.5f);

					cmd->viewangles.y += g_Vars.antiaim.distort_range * flDistortion;
					return;
				}

				if (bGenerate) {
					float flNormalised = std::remainderf(g_Vars.antiaim.distort_range, 180.f);

					flGenerated = RandomFloat(-flNormalised, flNormalised);
					bGenerate = false;
				}

				float flDelta = fabs(flLastMoveYaw - local->m_flLowerBodyYawTarget());
				cmd->viewangles.y += flDelta + flGenerated;
			}
		}
		else {
			bool bUsingManualAA = g_Vars.globals.manual_aa != -1;
			if (bUsingManualAA && g_Vars.antiaim.distort_manual_aa)
				return;

			if (bDoDistort) {
				if (true)
					cmd->viewangles.y += std::fmod(Interfaces::m_pGlobalVars->realtime * (g_Vars.antiaim.distort_speed * 2) * 100, 360.f);
			}
		}
	}

	C_AntiAimbot::Directions C_AntiAimbot::HandleDirection(Encrypted_t<CUserCmd> cmd) {
		const auto pLocal = C_CSPlayer::GetLocalPlayer();
		if (!pLocal)
			return Directions::YAW_NONE;

		// best target.
		struct AutoTarget_t { float fov; C_CSPlayer* player; };
		AutoTarget_t target{ 180.f + 1.f, nullptr };

		// iterate players, for closest distance.
		for (int i{ 1 }; i <= Interfaces::m_pGlobalVars->maxClients; ++i) {
			auto player = C_CSPlayer::GetPlayerByIndex(i);
			if (!player || player->IsDead())
				continue;

			if (player->IsDormant())
				continue;

			bool is_team = player->IsTeammate(pLocal);
			if (is_team)
				continue;

			auto lag_data = Engine::LagCompensation::Get()->GetLagData(player->m_entIndex);
			if (!lag_data.IsValid())
				continue;

			// get best target based on fov.
			Vector origin = player->m_vecOrigin();

			auto AngleDistance = [&](QAngle& angles, const Vector& start, const Vector& end) -> float {
				auto direction = end - start;
				auto aimAngles = direction.ToEulerAngles();
				auto delta = aimAngles - angles;
				delta.Normalize();

				return sqrtf(delta.x * delta.x + delta.y * delta.y);
			};

			float fov = AngleDistance(cmd->viewangles, g_Vars.globals.m_vecFixedEyePosition, origin);

			if (fov < target.fov) {
				target.fov = fov;
				target.player = player;
			}
		}

		// get best player.
		const auto player = target.player;
		if (!player)
			return Directions::YAW_NONE;

		Vector& bestOrigin = player->m_vecOrigin();

		// calculate direction from bestOrigin to our origin
		const auto yaw = Math::CalcAngle(bestOrigin, pLocal->m_vecOrigin());

		Vector forward, right, up;
		Math::AngleVectors(yaw, forward, right, up);

		Vector vecStart = pLocal->GetEyePosition();
		Vector vecEnd = vecStart + forward * 100.0f;

		Ray_t rightRay(vecStart + right * 35.0f, vecEnd + right * 35.0f), leftRay(vecStart - right * 35.0f, vecEnd - right * 35.0f);

		// setup trace filter
		CTraceFilter filter{ };
		filter.pSkip = pLocal;

		CGameTrace tr{ };

		m_pEngineTrace->TraceRay(rightRay, MASK_SOLID, &filter, &tr);
		float rightLength = (tr.endpos - tr.startpos).Length();

		m_pEngineTrace->TraceRay(leftRay, MASK_SOLID, &filter, &tr);
		float leftLength = (tr.endpos - tr.startpos).Length();

		static auto leftTicks = 0;
		static auto rightTicks = 0;
		static auto backTicks = 0;

		if (rightLength - leftLength > 20.0f)
			leftTicks++;
		else
			leftTicks = 0;

		if (leftLength - rightLength > 20.0f)
			rightTicks++;
		else
			rightTicks = 0;

		if (fabs(rightLength - leftLength) <= 20.0f)
			backTicks++;
		else
			backTicks = 0;

		Directions direction = Directions::YAW_NONE;

		if (rightTicks > 2) {
			direction = Directions::YAW_RIGHT;
		}
		else {
			if (leftTicks > 2) {
				direction = Directions::YAW_LEFT;
			}
			else {
				if (backTicks > 2)
					direction = Directions::YAW_BACK;
			}
		}

		return direction;
	}
}