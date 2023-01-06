#include "../Hooked.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../Features/Game/Prediction.hpp"
#include "../../Features/Miscellaneous/Movement.hpp"
#include <intrin.h>
#include "../../Features/Rage/Ragebot.hpp"
#include "../../Features/Miscellaneous/Miscellaneous.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../SDK/Classes/Exploits.hpp"
#include "../../Features/Rage/FakeLag.hpp"
#include "../../Features/Rage/LagCompensation.hpp"
#include "../../Utils/Threading/threading.h"
#include "../../SDK/Classes/CCSGO_HudDeathNotice.hpp"
#include "../../Features/Rage/ShotInformation.hpp"
#include <thread>
#include "../../Features/Visuals/GrenadePrediction.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"
#include "../../Features/Game/SetupBones.hpp"
#include "../../Features/Visuals/Hitmarker.hpp"
#include "../../Features/Rage/AntiAim.hpp"

extern float fl_Override;
extern bool g_Override;

Vector AutoPeekPos;

int LastShotTime = 0;
int OutgoingTickcount = 0;

void PreserveKillfeed() {
	auto local = C_CSPlayer::GetLocalPlayer();

	if (!local || !Interfaces::m_pEngine->IsInGame() || !Interfaces::m_pEngine->IsConnected()) {
		return;
	}

	static auto status = false;
	static float m_spawn_time = local->m_flSpawnTime();

	auto set = false;
	if (m_spawn_time != local->m_flSpawnTime() || status != g_Vars.esp.preserve_killfeed) {
		set = true;
		status = g_Vars.esp.preserve_killfeed;
		m_spawn_time = local->m_flSpawnTime();
	}

	for (int i = 0; i < Interfaces::g_pDeathNotices->m_vecDeathNotices.Count(); i++) {
		auto cur = &Interfaces::g_pDeathNotices->m_vecDeathNotices[i];
		if (!cur) {
			continue;
		}

		if (local->IsDead() || set) {
			if (cur->set != 1.f && !set) {
				continue;
			}

			cur->m_flStartTime = Interfaces::m_pGlobalVars->curtime;
			cur->m_flStartTime -= local->m_iHealth() <= 0 ? 2.f : 7.5f;
			cur->set = 2.f;

			continue;
		}

		if (cur->set == 2.f) {
			continue;
		}

		if (!status) {
			cur->set = 1.f;
			return;
		}

		if (cur->set == 1.f) {
			continue;
		}

		if (cur->m_flLifeTimeModifier == 1.5f) {
			cur->m_flStartTime = FLT_MAX;
		}

		cur->set = 1.f;
	}
}

namespace Hooked
{
	inline float anglemod(float a)
	{
		a = (360.f / 65536) * ((int)(a * (65536.f / 360.0f)) & 65535);
		return a;
	}

	// BUGBUG: Why doesn't this call angle diff?!?!?
	float ApproachAngle(float target, float value, float speed)
	{
		target = anglemod(target);
		value = anglemod(value);

		float delta = target - value;

		// Speed is assumed to be positive
		if (speed < 0)
			speed = -speed;

		if (delta < -180)
			delta += 360;
		else if (delta > 180)
			delta -= 360;

		if (delta > speed)
			value += speed;
		else if (delta < -speed)
			value -= speed;
		else
			value = target;

		return value;
	}


	// BUGBUG: Why do we need both of these?
	float AngleDiff(float destAngle, float srcAngle)
	{
		float delta;

		delta = fmodf(destAngle - srcAngle, 360.0f);
		if (destAngle > srcAngle)
		{
			if (delta >= 180)
				delta -= 360;
		}
		else
		{
			if (delta <= -180)
				delta += 360;
		}
		return delta;
	}

	class NetPos {
	public:
		float  m_time;
		Vector m_pos;

	public:
		__forceinline NetPos() : m_time{ }, m_pos{ } {};
		__forceinline NetPos(float time, Vector pos) : m_time{ time }, m_pos{ pos } {};
	};

	void UpdateInformation(CUserCmd* cmd) {

		auto local = C_CSPlayer::GetLocalPlayer();

		if (!local)
			return;

		if (Interfaces::m_pClientState->m_nChokedCommands() > 0)
			return;

		local->m_fEffects() &= ~EF_NOINTERP;


		CCSGOPlayerAnimState* state = local->m_PlayerAnimState();
		if (!state)
			return;

		// update time.
		g_Vars.globals.m_flAnimFrame = TICKS_TO_TIME(local->m_nTickBase()) - g_Vars.globals.m_flAnimTime;
		g_Vars.globals.m_flAnimTime = TICKS_TO_TIME(local->m_nTickBase());

		// current angle will be animated.
		g_Vars.globals.RegularAngles = cmd->viewangles;


		Math::Clamp(g_Vars.globals.RegularAngles.x, -90.f, 90.f);
		g_Vars.globals.RegularAngles.Normalize();

		// write angles to model.
		Interfaces::m_pPrediction->SetLocalViewAngles(g_Vars.globals.RegularAngles);

		// set lby to predicted value.
		local->m_flLowerBodyYawTarget() = g_Vars.globals.m_flBody;

		// CCSGOPlayerAnimState::Update, bypass already animated checks.
		if (state->m_nLastFrame == Interfaces::m_pGlobalVars->framecount)
			state->m_nLastFrame -= 1;

		local->m_iEFlags() &= ~(EFL_DIRTY_ABSTRANSFORM | EFL_DIRTY_ABSVELOCITY);

		state->m_flFeetYawRate = 0.f;

		local->UpdateClientSideAnimationEx();

		auto flWeight12Backup = local->m_AnimOverlay().Element(12).m_flWeight;

		local->m_AnimOverlay().Element(12).m_flWeight = 0.f;

		// land fall anim fix.
		if (FL_ONGROUND) {
			// they are on ground.
			state->m_bOnGround = true;
			// no they didnt land.
			state->m_bLand = false;
		}

		bool bOnGround = local->m_fFlags() & FL_ONGROUND;
		bool bJumped = false;
		bool bLandedOnServer = false;
		float flLandTime = 0.f;
		float m_anim_time = Interfaces::m_pGlobalVars->curtime;

		if (FL_ONGROUND) {
			flLandTime = m_anim_time - float(local->m_AnimOverlay().Element(4).m_flPlaybackRate / local->m_AnimOverlay().Element(4).m_flCycle);
			bLandedOnServer = flLandTime >= m_anim_time;
		}

		if (bLandedOnServer && !bJumped) {
			if (flLandTime <= m_anim_time) {
				bJumped = true;
				bOnGround = true;
			}
			else {
				bOnGround = local->m_fFlags() & FL_ONGROUND;
			}
		}

		if (bOnGround) {
			local->m_fFlags() |= FL_ONGROUND;
		}
		else {
			local->m_fFlags() &= ~FL_ONGROUND;
		}

		local->UpdateClientSideAnimation();


		// pull the lower body direction towards the eye direction, but only when the player is moving
		// we landed.
		if (!g_Vars.globals.m_bGround && state->m_bOnGround) {
			g_Vars.globals.m_flBody = g_Vars.globals.RegularAngles.y;
			g_Vars.globals.m_flBodyPred = g_Vars.globals.m_flAnimTime;
		}

		// walking, delay lby update by .22.
		else if (local->m_vecVelocity().Length() > 0.1f) {
			if (state->m_bOnGround)
				g_Vars.globals.m_flBody = g_Vars.globals.RegularAngles.y;

			g_Vars.globals.m_flBodyPred = g_Vars.globals.m_flAnimTime + 0.22f;
		}

		// standing update every 1.1s
		else if (g_Vars.globals.m_flAnimTime > g_Vars.globals.m_flBodyPred && state->m_bOnGround) {
			g_Vars.globals.m_flBody = g_Vars.globals.RegularAngles.y;
			g_Vars.globals.m_flBodyPred = g_Vars.globals.m_flAnimTime + 1.1f;
		}

		// build bones at the end of everything

		g_BoneSetup.BuildBones(local, BONE_USED_BY_ANYTHING, BoneSetupFlags::None);

		g_Vars.globals.flRealYaw = state->m_flAbsRotation;
		g_Vars.globals.angViewangles = cmd->viewangles;

		// copy real bone positions
		auto boneCount = local->m_CachedBoneData().Count();
		std::memcpy(g_Vars.globals.m_RealBonesPositions, local->m_vecBonePos(), boneCount * sizeof(Vector));
		std::memcpy(g_Vars.globals.m_RealBonesRotations, local->m_quatBoneRot(), boneCount * sizeof(Quaternion));

		local->m_AnimOverlay().Element(12).m_flWeight = flWeight12Backup;
		if (g_Vars.globals.m_flPoseParams) {
			std::memcpy(local->m_flPoseParameter(), g_Vars.globals.m_flPoseParams, sizeof(local->m_flPoseParameter()));
		}

		if (local->m_CachedBoneData().Base() != local->m_BoneAccessor().m_pBones) {
			std::memcpy(local->m_BoneAccessor().m_pBones, local->m_CachedBoneData().Base(), local->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
		}

		// save updated data.
		g_Vars.globals.m_bGround = state->m_bOnGround;
		g_Vars.globals.m_fFlags = local->m_fFlags();
		g_Vars.globals.m_speed = local->m_PlayerAnimState()->m_velocity;
	}

	std::deque< NetPos >   m_net_pos;
	bool CreateMoveHandler(float ft, CUserCmd* _cmd, bool* bSendPacket, bool* bFinalTick) {
		auto bRet = oCreateMove(ft, _cmd);

		g_Vars.globals.m_bInCreateMove = true;

		auto pLocal = C_CSPlayer::GetLocalPlayer();
		if (!pLocal || pLocal->IsDead()) {
			g_Vars.globals.WasShootingInPeek = false;
			AutoPeekPos.Set();

			Engine::Prediction::Instance().Invalidate();
			g_Vars.globals.m_bInCreateMove = false;
			return bRet;
		}

		auto weapon = (C_WeaponCSBaseGun*)(pLocal->m_hActiveWeapon().Get());
		if (!weapon) {
			Engine::Prediction::Instance().Invalidate();
			g_Vars.globals.m_bInCreateMove = false;

			return bRet;
		}


		g_Vars.globals.m_vecUnpredictedVel = pLocal->m_vecVelocity();

		g_Vars.globals.m_flCurtime = Interfaces::m_pGlobalVars->curtime;

		Encrypted_t<CUserCmd> cmd(_cmd);

		static auto m_iCrosshairData = Interfaces::m_pCvar->FindVar(XorStr("weapon_debug_spread_show"));
		if (g_Vars.esp.force_sniper_crosshair && m_iCrosshairData) {
			m_iCrosshairData->SetValue(!pLocal->m_bIsScoped() ? 3 : 0);
		}
		else {
			if (m_iCrosshairData)
				m_iCrosshairData->SetValue(0);
		}

		static auto g_GameRules = *(uintptr_t**)(Engine::Displacement.Data.m_GameRules);
		bool invalid = g_GameRules && *(bool*)(*(uintptr_t*)g_GameRules + 0x20) || (pLocal->m_fFlags() & (1 << 6));

		Encrypted_t<CVariables::GLOBAL> globals(&g_Vars.globals);

		static QAngle lockedAngles = QAngle();


		auto movement = Interfaces::Movement::Get();

		if (g_Vars.globals.menuOpen) {
			// just looks nicer
			auto RemoveButtons = [&](int key) { cmd->buttons &= ~key; };
			RemoveButtons(IN_ATTACK);
			RemoveButtons(IN_ATTACK2);
			RemoveButtons(IN_USE);

			if (GUI::ctx->typing) {
				RemoveButtons(IN_MOVERIGHT);
				RemoveButtons(IN_MOVELEFT);
				RemoveButtons(IN_FORWARD);
				RemoveButtons(IN_BACK);

				movement->InstantStop(cmd.Xor());
			}
		}

		g_Vars.globals.m_pCmd = cmd.Xor();

		auto weaponInfo = weapon->GetCSWeaponData();

		g_Vars.globals.bCanWeaponFire = pLocal->CanShoot();

		//g_TickbaseController.PreMovement( );

		g_Vars.globals.m_lerp = Engine::LagCompensation::Get()->GetLerp();


		Engine::Prediction::Instance()->RunGamePrediction();

		auto& prediction = Engine::Prediction::Instance();

		movement->PrePrediction(cmd, pLocal, bSendPacket, bFinalTick, nullptr);
		prediction.Begin(cmd, bSendPacket, cmd->command_number);
		{
			g_Vars.globals.m_bAimbotShot = false;

			if (g_Vars.misc.autopeek && g_Vars.misc.autopeek_bind.enabled) {
				if ((pLocal->m_fFlags() & FL_ONGROUND)) {
					if (AutoPeekPos.IsZero()) {
						AutoPeekPos = pLocal->GetAbsOrigin();
					}
				}
			}
			else {
				AutoPeekPos = Vector();
			}

			movement->InPrediction();
			movement->PostPrediction();

			g_Vars.globals.m_vecVelocity = pLocal->m_vecVelocity();
			/*
			if( !g_Vars.misc.slide_walk ) {
				if( pLocal->m_MoveType( ) != MOVETYPE_LADDER && pLocal->m_MoveType( ) != MOVETYPE_NOCLIP && pLocal->m_MoveType( ) != MOVETYPE_FLY )
					cmd->buttons &= ~( IN_FORWARD | IN_BACK | IN_MOVERIGHT | IN_MOVELEFT );
			}
			else {
				if( pLocal->m_MoveType( ) != MOVETYPE_LADDER && pLocal->m_fFlags( ) & FL_ONGROUND ) {
					if( cmd->forwardmove > 0 ) {
						cmd->buttons |= IN_BACK;
						cmd->buttons &= ~IN_FORWARD;
					}

					if( cmd->forwardmove < 0 ) {
						cmd->buttons |= IN_FORWARD;
						cmd->buttons &= ~IN_BACK;
					}

					if( cmd->sidemove < 0 ) {
						cmd->buttons |= IN_MOVERIGHT;
						cmd->buttons &= ~IN_MOVELEFT;
					}

					if( cmd->sidemove > 0 ) {
						cmd->buttons |= IN_MOVELEFT;
						cmd->buttons &= ~IN_MOVERIGHT;
					}
				}
			}*/

			int nShotCmd = -1;

			if (cmd->buttons & IN_ATTACK
				&& weapon->m_iItemDefinitionIndex() != WEAPON_C4
				&& weaponInfo->m_iWeaponType >= WEAPONTYPE_KNIFE
				&& weaponInfo->m_iWeaponType <= WEAPONTYPE_MACHINEGUN
				&& pLocal->CanShoot())
			{
				nShotCmd = cmd->command_number;
				g_Vars.globals.m_iShotTick = cmd->tick_count;
				lockedAngles = cmd->viewangles;
				LastShotTime = Interfaces::m_pGlobalVars->tickcount;

				if (weaponInfo->m_iWeaponType != WEAPONTYPE_KNIFE && weaponInfo->m_iWeaponType != WEAPONTYPE_GRENADE) {
					g_Vars.globals.m_flLastShotTime = Interfaces::m_pGlobalVars->realtime;
					//if( g_Vars.globals.bInRagebot ) {
					//	g_Vars.globals.m_flLastShotTimeInRage = g_Vars.globals.m_flLastShotTime;
					//}
				}

				g_Vars.globals.WasShootingInChokeCycle = !(*bSendPacket);
				g_Vars.globals.WasShooting = true;

				if (weaponInfo->m_iWeaponType != WEAPONTYPE_KNIFE)
					g_Vars.globals.WasShootingInPeek = true;

				//g_Vars.globals.m_ShotAngle = Interfaces::m_pInput->m_pCommands[ nShotCmd % 150 ].viewangles;

			}
			else {
				g_Vars.globals.WasShooting = false;
			}

			g_Vars.globals.iWeaponIndex = weapon->m_iItemDefinitionIndex();

			g_Vars.globals.m_flPreviousDuckAmount = pLocal->m_flDuckAmount();

			Engine::C_ShotInformation::Get()->CorrectSnapshots(*bSendPacket);

			bool bInAttack = cmd->buttons & IN_ATTACK;
			bool bCanShoot = TICKS_TO_TIME(pLocal->m_nTickBase()) >= weapon->m_flNextPrimaryAttack();

			if (!(TICKS_TO_TIME(pLocal->m_nTickBase()) >= pLocal->m_flNextAttack())) {
				bCanShoot = false;
			}

			if (weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
				bCanShoot = false;

			if (bCanShoot && bInAttack && weaponInfo->m_iWeaponType != WEAPONTYPE_GRENADE && weaponInfo->m_iWeaponType != WEAPONTYPE_C4) {
				if (!g_Vars.globals.Fakewalking)
					*bSendPacket = false;
			}

			UpdateInformation(cmd.Xor());

			g_Vars.globals.m_bOldShot = g_Vars.globals.m_bAimbotShot;

			//	g_TickbaseController.PostMovement( bSendPacket, cmd.Xor( ) );
		}
		prediction.End();

		if (!g_Vars.misc.slide_walk)
			if (pLocal->m_MoveType() != MOVETYPE_LADDER && pLocal->m_MoveType() != MOVETYPE_NOCLIP && pLocal->m_MoveType() != MOVETYPE_FLY)
				cmd->buttons &= ~(IN_FORWARD | IN_BACK | IN_MOVERIGHT | IN_MOVELEFT);

		if (g_Vars.antiaim.enabled && g_Vars.antiaim.manual && g_Vars.antiaim.mouse_override.enabled) {
			pLocal->pl().v_angle = globals->PreviousViewangles;
		}

		if (*bSendPacket) {
			g_Vars.globals.WasShootingInChokeCycle = false;

			g_Vars.globals.LastChokedCommands = Interfaces::m_pClientState->m_nChokedCommands();

			if (g_Vars.globals.FixCycle) {
				g_Vars.globals.FixCycle = false;
				g_Vars.globals.UnknownCycleFix = true;
			}

			OutgoingTickcount = Interfaces::m_pGlobalVars->tickcount;

			// TODO: make this lag compensated
			g_Vars.globals.m_iNetworkedTick = pLocal->m_nTickBase();
			g_Vars.globals.m_vecNetworkedOrigin = pLocal->m_vecOrigin();
		}

		if (!*bSendPacket || !*bFinalTick)
			g_Vars.globals.RegularAngles = cmd->viewangles;
		else
			g_Vars.globals.FakeAngle = cmd->viewangles;

		if (*bSendPacket) {
			Vector cur = pLocal->m_vecOrigin();
			Vector prev = m_net_pos.empty() ? cur : m_net_pos.front().m_pos;

			g_Vars.globals.bBrokeLC = (cur - prev).LengthSquared() > 4096.f;
			g_Vars.globals.delta = std::clamp((cur - prev).LengthSquared(), 0.f, 4096.f);

			m_net_pos.emplace_front(Interfaces::m_pGlobalVars->curtime, cur);
		}

		g_Vars.globals.bFinalPacket = *bSendPacket;

		if (g_Vars.misc.anti_untrusted) {
			cmd->viewangles.Normalize();
			cmd->viewangles.Clamp();
		}

		g_Vars.globals.m_bInCreateMove = false;

		return false;
	}

	bool __stdcall CreateMove(float ft, CUserCmd* _cmd) {
		g_Vars.globals.szLastHookCalled = XorStr("2");
		if (!_cmd || !_cmd->command_number)
			return oCreateMove(ft, _cmd);

		if (g_Vars.cl_csm_shadows->GetInt() != 0)
			g_Vars.cl_csm_shadows->SetValue(0);

		if (g_Vars.engine_no_focus_sleep->GetInt() != 0)
			g_Vars.engine_no_focus_sleep->SetValue(0);

		PreserveKillfeed();

		Encrypted_t<uintptr_t> pAddrOfRetAddr((uintptr_t*)_AddressOfReturnAddress());
		bool* bFinalTick = reinterpret_cast<bool*>(uintptr_t(pAddrOfRetAddr.Xor()) + 0x15);
		bool* bSendPacket = reinterpret_cast<bool*>(uintptr_t(pAddrOfRetAddr.Xor()) + 0x14);

		if (!(*bSendPacket))
			*bSendPacket = true;

		if (!*bFinalTick)
			*bSendPacket = false;

		int iLagLimit = 16;
		g_Vars.fakelag.iLagLimit = std::clamp(iLagLimit, 0, 16);

		auto result = CreateMoveHandler(ft, _cmd, bSendPacket, bFinalTick);

		Engine::Prediction::Instance()->KeepCommunication(bSendPacket, _cmd->command_number);

		auto pLocal = C_CSPlayer::GetLocalPlayer();
		if (!g_Vars.globals.HackIsReady || !pLocal || !Interfaces::m_pEngine->IsInGame()) {
			Engine::Prediction::Instance().Invalidate();
			return oCreateMove(ft, _cmd);
		}

		return result;
	}

	bool __cdecl ReportHit(Hit_t* hit) {
		if ((g_Vars.esp.visualize_hitmarker_world || g_Vars.esp.visualize_damage) && hit) {
			Hitmarkers::AddWorldHitmarker(hit->x, hit->y, hit->z);
		}

		return oReportHit(hit);
	}

	bool __cdecl IsUsingStaticPropDebugMode()
	{
		if (Interfaces::m_pEngine.IsValid() && !Interfaces::m_pEngine->IsInGame())
			return oIsUsingStaticPropDebugMode();

		return g_Vars.esp.night_mode;
	}

	void __fastcall RunSimulation(void* this_, void*, int iCommandNumber, CUserCmd* pCmd, size_t local) {
		g_TickbaseController.OnRunSimulation(this_, iCommandNumber, pCmd, local);
	}

	void __fastcall PredictionUpdate(void* prediction, void*, int startframe, bool validframe, int incoming_acknowledged, int outgoing_command) {
		g_TickbaseController.OnPredictionUpdate(prediction, nullptr, startframe, validframe, incoming_acknowledged, outgoing_command);
	}
}
