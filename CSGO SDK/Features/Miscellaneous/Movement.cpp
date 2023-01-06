#include "../../source.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "Movement.hpp"
#include "../../Utils/Math.h"
#include "../Rage/AntiAim.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../Utils/InputSys.hpp"
#include "../Rage/FakeLag.hpp"
#include "../Rage/Ragebot.hpp"
#include "../Rage/Zeusbot.hpp"
#include "../Rage/AntiAim.hpp"
#include "../Rage/KnifeBot.hpp"
#include "../Game/Prediction.hpp"
#include "../../SDK/Displacement.hpp"
#include "../Game/SetupBones.hpp"
#include "../../Utils/Threading/threading.h"
#include "../Rage/TickbaseShift.hpp"
#include "../Visuals/EventLogger.hpp"
#include "../Rage/Ragebot.hpp"

// todo: move this
C_AnimationLayer FakeAnimLayers[13];

extern Vector AutoPeekPos;

extern matrix3x4_t HeadBone;

void SimulateMovement(C_SimulationData& data) {
	if (!(data.m_iFlags & FL_ONGROUND)) {
		data.m_vecVeloctity.z -= (Interfaces::m_pGlobalVars->interval_per_tick * g_Vars.sv_gravity->GetFloat() * 0.5f);
	}
	else if (data.m_bJumped) {
		data.m_bJumped = false;
		data.m_vecVeloctity.z = g_Vars.sv_jump_impulse->GetFloat();
		data.m_iFlags &= ~FL_ONGROUND;
	}

	// can't step up onto very steep slopes
	static const float MIN_STEP_NORMAL = 0.7f;

	if (!data.m_vecVeloctity.IsZero()) {
		auto collidable = data.m_player->GetCollideable();
		const Vector mins = collidable->OBBMins();
		const Vector max = collidable->OBBMaxs();

		const Vector src = data.m_vecOrigin;
		Vector end = src + (data.m_vecVeloctity * Interfaces::m_pGlobalVars->interval_per_tick);

		Ray_t ray;
		ray.Init(src, end, mins, max);

		CGameTrace trace;
		CTraceFilter filter;
		filter.pSkip = data.m_player;

		Interfaces::m_pEngineTrace->TraceRay(ray, MASK_SOLID, &filter, &trace);

		// CGameMovement::TryPlayerMove
		if (trace.fraction != 1.f) {
			// BUGFIXME: is it should be 4? ( not 2 )
			for (int i = 0; i < 2; i++) {
				// decompose velocity into plane
				data.m_vecVeloctity -= trace.plane.normal * data.m_vecVeloctity.Dot(trace.plane.normal);

				const float dot = data.m_vecVeloctity.Dot(trace.plane.normal);
				if (dot < 0.f) { // moving against plane
					data.m_vecVeloctity.x -= dot * trace.plane.normal.x;
					data.m_vecVeloctity.y -= dot * trace.plane.normal.y;
					data.m_vecVeloctity.z -= dot * trace.plane.normal.z;
				}

				end = trace.endpos + (data.m_vecVeloctity * (Interfaces::m_pGlobalVars->interval_per_tick * (1.f - trace.fraction)));

				ray.Init(trace.endpos, end, mins, max);
				Interfaces::m_pEngineTrace->TraceRay(ray, MASK_SOLID, &filter, &trace);
				if (trace.fraction == 1.f)
					break;
			}
		}

		data.m_vecOrigin = trace.endpos;
		end = trace.endpos;
		end.z -= 2.f;

		ray.Init(data.m_vecOrigin, end, mins, max);
		Interfaces::m_pEngineTrace->TraceRay(ray, MASK_SOLID, &filter, &trace);

		data.m_iFlags &= ~FL_ONGROUND;

		if (trace.DidHit() && trace.plane.normal.z >= MIN_STEP_NORMAL) {
			data.m_iFlags |= FL_ONGROUND;
		}

		if (data.m_iFlags & FL_ONGROUND)
			data.m_vecVeloctity.z = 0.0f;
		else
			data.m_vecVeloctity.z -= Interfaces::m_pGlobalVars->interval_per_tick * g_Vars.sv_gravity->GetFloat() * 0.5f;
	}
}

void ExtrapolatePlayer(C_SimulationData& data, int simulationTicks, const Vector& wishvel, float wishspeed, float maxSpeed) {
	for (int i = 0; i < simulationTicks; ++i) {
		SimulateMovement(data);

		if (data.m_vecVeloctity.IsZero())
			break;

		auto Accelerate = [&data](const Vector& wishdir, float wishspeed, float maxSpeed) {
			float addspeed, accelspeed, currentspeed;

			// See if we are changing direction a bit
			currentspeed = data.m_vecVeloctity.Dot(wishdir);

			// Reduce wishspeed by the amount of veer.
			addspeed = wishspeed - currentspeed;

			// If not going to add any speed, done.
			if (addspeed <= 0)
				return;

			// Determine amount of accleration.
			static int playerSurfaceFrictionOffset = SDK::Memory::FindInDataMap(data.m_player->GetPredDescMap(), XorStr("m_surfaceFriction"));
			float playerSurfaceFriction = *(float*)(uintptr_t(data.m_player) + playerSurfaceFrictionOffset);
			accelspeed = g_Vars.sv_accelerate->GetFloat() * Interfaces::m_pGlobalVars->interval_per_tick * maxSpeed * playerSurfaceFriction;

			// Cap at addspeed
			if (accelspeed > addspeed)
				accelspeed = addspeed;

			// Adjust velocity.
			for (int i = 0; i < 3; i++) {
				data.m_vecVeloctity[i] += accelspeed * wishdir[i];
			}

			float len = data.m_vecVeloctity.Length2D();
			if (len > ((data.m_iFlags & FL_ONGROUND) ? (maxSpeed) : 320.0f)) {
				float z = data.m_vecVeloctity.z;
				data.m_vecVeloctity.z = 0.0f;
				data.m_vecVeloctity = data.m_vecVeloctity.Normalized() * ((data.m_iFlags & FL_ONGROUND) ? (maxSpeed) : 320.0f);
				data.m_vecVeloctity.z = z;
			}
		};

		Accelerate(wishvel, wishspeed, maxSpeed);
	}
}

void RotateMovement(Encrypted_t<CUserCmd> cmd, QAngle wish_angle, QAngle old_angles) {
	// aimware movement fix, reversed by ph4ge/senator for gucci
	if (old_angles.x != wish_angle.x || old_angles.y != wish_angle.y || old_angles.z != wish_angle.z) {
		Vector wish_forward, wish_right, wish_up, cmd_forward, cmd_right, cmd_up;

		auto viewangles = old_angles;
		auto movedata = Vector(cmd->forwardmove, cmd->sidemove, cmd->upmove);
		viewangles.Normalize();

		if (viewangles.z != 0.f) {
			auto pLocal = C_CSPlayer::GetLocalPlayer();

			if (pLocal && !(pLocal->m_fFlags() & FL_ONGROUND))
				movedata.y = 0.f;
		}

		wish_forward = wish_angle.ToVectors(&wish_right, &wish_up);
		cmd_forward = viewangles.ToVectors(&cmd_right, &cmd_up);

		auto v8 = sqrt(wish_forward.x * wish_forward.x + wish_forward.y * wish_forward.y), v10 = sqrt(wish_right.x * wish_right.x + wish_right.y * wish_right.y), v12 = sqrt(wish_up.z * wish_up.z);

		Vector wish_forward_norm(1.0f / v8 * wish_forward.x, 1.0f / v8 * wish_forward.y, 0.f),
			wish_right_norm(1.0f / v10 * wish_right.x, 1.0f / v10 * wish_right.y, 0.f),
			wish_up_norm(0.f, 0.f, 1.0f / v12 * wish_up.z);

		auto v14 = sqrt(cmd_forward.x * cmd_forward.x + cmd_forward.y * cmd_forward.y), v16 = sqrt(cmd_right.x * cmd_right.x + cmd_right.y * cmd_right.y), v18 = sqrt(cmd_up.z * cmd_up.z);

		Vector cmd_forward_norm(1.0f / v14 * cmd_forward.x, 1.0f / v14 * cmd_forward.y, 1.0f / v14 * 0.0f),
			cmd_right_norm(1.0f / v16 * cmd_right.x, 1.0f / v16 * cmd_right.y, 1.0f / v16 * 0.0f),
			cmd_up_norm(0.f, 0.f, 1.0f / v18 * cmd_up.z);

		auto v22 = wish_forward_norm.x * movedata.x, v26 = wish_forward_norm.y * movedata.x, v28 = wish_forward_norm.z * movedata.x, v24 = wish_right_norm.x * movedata.y, v23 = wish_right_norm.y * movedata.y, v25 = wish_right_norm.z * movedata.y, v30 = wish_up_norm.x * movedata.z, v27 = wish_up_norm.z * movedata.z, v29 = wish_up_norm.y * movedata.z;

		cmd->forwardmove = cmd_forward_norm.x * v24 + cmd_forward_norm.y * v23 + cmd_forward_norm.z * v25 + (cmd_forward_norm.x * v22 + cmd_forward_norm.y * v26 + cmd_forward_norm.z * v28) + (cmd_forward_norm.y * v30 + cmd_forward_norm.x * v29 + cmd_forward_norm.z * v27);
		cmd->sidemove = cmd_right_norm.x * v24 + cmd_right_norm.y * v23 + cmd_right_norm.z * v25 + (cmd_right_norm.x * v22 + cmd_right_norm.y * v26 + cmd_right_norm.z * v28) + (cmd_right_norm.x * v29 + cmd_right_norm.y * v30 + cmd_right_norm.z * v27);
		cmd->upmove = cmd_up_norm.x * v23 + cmd_up_norm.y * v24 + cmd_up_norm.z * v25 + (cmd_up_norm.x * v26 + cmd_up_norm.y * v22 + cmd_up_norm.z * v28) + (cmd_up_norm.x * v30 + cmd_up_norm.y * v29 + cmd_up_norm.z * v27);

		/*
		cmd->forwardmove = Math::Clamp( cmd->forwardmove, -g_Vars.cl_forwardspeed->GetFloat( ), g_Vars.cl_forwardspeed->GetFloat( ) );
		cmd->sidemove = Math::Clamp( cmd->sidemove, -g_Vars.cl_sidespeed->GetFloat( ), g_Vars.cl_sidespeed->GetFloat( ) );
		cmd->upmove = Math::Clamp( cmd->upmove, -g_Vars.cl_upspeed->GetFloat( ), g_Vars.cl_upspeed->GetFloat( ) );*/
	}
}

namespace Interfaces
{
	struct MovementData {
		Encrypted_t<CUserCmd> m_pCmd = nullptr;
		C_CSPlayer* m_pLocal = nullptr;
		bool* m_pSendPacket = nullptr;
		bool* m_pFinalPacket = nullptr;

		uintptr_t* m_pCLMoveReturn = nullptr;

		bool m_bInRecursion = false;
		int m_iRecursionTicks = 0;

		QAngle m_angRenderViewangles{ };
		QAngle m_angPreviousAngles{ };
		QAngle m_angMovementAngle{ };
		float m_flOldYaw = 0.0f;
		float m_flJumpFall = 0.0f;

		int nBhops = 0;
		int m_fPrePredFlags = 0;

		C_AnimationLayer* ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB;
		C_AnimationLayer* ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL;

		int   m_bStopPlayer = 0;
		bool  m_bMinimalSpeed = false;
		bool  m_bFastStop = false;
		bool  m_bStoppedTick = false;
		bool  m_bFakeducking = false;
		bool  m_bStopOk = false;
		bool  jumped_last_tick = false;
		bool  should_fake_jump = false;

		float m_flLowerBodyUpdateTime = 0.0f;

		int m_iLatency = 0;
	};

	static MovementData _move_data;

	class C_Movement : public Movement {
	public:
		virtual void PrePrediction(Encrypted_t<CUserCmd> cmd, C_CSPlayer* pLocal, bool* pSendPacket, bool* bFinalPacket, uintptr_t* cl_move);
		virtual void InPrediction();
		virtual void PostPrediction();
		virtual void InstantStop(CUserCmd* cmd = nullptr);
		virtual void ThirdPerson();

		virtual float GetLBYUpdateTime() {
			return  m_movement_data->m_flLowerBodyUpdateTime;
		}

		virtual int GetButtons() {
			return m_movement_data->m_pCmd->buttons;
		}

		virtual QAngle& GetMovementAngle() {
			return m_movement_data->m_angMovementAngle;
		}

		virtual bool StopPlayer() {
			if (m_movement_data->m_bStoppedTick) {
				return false;
			}

			return AutoStopInternal();
		}

		virtual void StopPlayerAtMinimalSpeed() {
			m_movement_data->m_bMinimalSpeed = true;
		}

		virtual void AutoStop(int ticks = 1);
		virtual bool GetStopState();
		virtual bool CreateMoveRecursion();

		C_Movement() : m_movement_data(&_move_data) { ; };
		virtual ~C_Movement() { };

	private:
		Encrypted_t<MovementData> m_movement_data;

		void MoveExploit();
		bool AutoStopInternal();
		bool AdjustVelocity();
		void AutoJump();
		void AutoStrafe();
		void SlowWalk(Encrypted_t<CUserCmd> cmd);
	};

	Movement* Movement::Get() {
		static C_Movement movement;
		return &movement;
	}

	void C_Movement::PrePrediction(Encrypted_t<CUserCmd> cmd, C_CSPlayer* pLocal, bool* pSendPacket, bool* bFinalPacket, uintptr_t* cl_move) {
		m_movement_data->m_pCmd = cmd;
		m_movement_data->m_pLocal = pLocal;
		m_movement_data->m_pSendPacket = pSendPacket;
		m_movement_data->m_pFinalPacket = bFinalPacket;
		m_movement_data->m_angRenderViewangles = m_movement_data->m_pCmd->viewangles;

		m_movement_data->m_pCLMoveReturn = cl_move;
		m_movement_data->m_bStopOk = false;
		m_movement_data->m_bStoppedTick = false;

		// static int autostop_ticks = 0;

		if (!pLocal || pLocal->IsDead())
			return;

		{
			float delta = std::fabsf(Interfaces::m_pEngine->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING) - Interfaces::m_pEngine->GetNetChannelInfo()->GetAvgLatency(FLOW_OUTGOING));
			m_movement_data->m_iLatency = TIME_TO_TICKS(delta) + 1 - **(int**)Engine::Displacement.Data.m_uHostFrameTicks;
		}

		m_movement_data->m_angMovementAngle = m_movement_data->m_pCmd->viewangles;
		m_movement_data->m_fPrePredFlags = m_movement_data->m_pLocal->m_fFlags();

		if (g_Vars.misc.fastduck)
			m_movement_data->m_pCmd->buttons |= IN_BULLRUSH;

		if (g_Vars.misc.duckjump) {
			if (m_movement_data->m_pCmd->buttons & IN_JUMP) {
				if (!(m_movement_data->m_pLocal->m_fFlags() & FL_ONGROUND))
					m_movement_data->m_pCmd->buttons |= IN_DUCK;
			}
		}

		if (g_Vars.misc.minijump && !((m_movement_data->m_pCmd->buttons & IN_DUCK))) {
			if (!(m_movement_data->m_pCmd->buttons & IN_JUMP) || (m_movement_data->m_pLocal->m_fFlags() & FL_ONGROUND) || m_movement_data->m_pLocal->m_vecVelocity().z >= -140.0f)
				m_movement_data->m_pCmd->buttons &= ~IN_DUCK;
			else
				m_movement_data->m_pCmd->buttons |= IN_DUCK;
		}

		g_Vars.globals.Fakewalking = false;

		if (g_Vars.misc.autojump)
			AutoJump();

		if (g_Vars.misc.autostrafer)
			AutoStrafe();

		bool instant_stop = (g_Vars.misc.instant_stop && g_Vars.misc.instant_stop_key.enabled);
		if (instant_stop || (g_Vars.misc.quickstop && !g_Vars.globals.WasShootingInPeek && m_movement_data->m_pLocal->m_fFlags() & FL_ONGROUND && !(m_movement_data->m_pCmd->buttons & IN_JUMP) && m_movement_data->m_pLocal->m_vecVelocity().Length() >= 1.2f)) {
			if (instant_stop || (!(m_movement_data->m_pCmd->buttons & IN_JUMP) && m_movement_data->m_pCmd->forwardmove == m_movement_data->m_pCmd->sidemove && m_movement_data->m_pCmd->sidemove == 0.0f)) {
				m_movement_data->m_bStopPlayer = 1;
				m_movement_data->m_bMinimalSpeed = false;
			}
		}

		bool peek = true;
		if (g_Vars.misc.autopeek && g_Vars.globals.WasShootingInPeek && !AutoPeekPos.IsZero()) {
			cmd->buttons &= ~IN_JUMP;

			auto delta = AutoPeekPos - m_movement_data->m_pLocal->GetAbsOrigin();
			m_movement_data->m_angMovementAngle = delta.ToEulerAngles();

			peek = false;

			m_movement_data->m_pCmd->forwardmove = g_Vars.cl_forwardspeed->GetFloat();
			m_movement_data->m_pCmd->sidemove = 0.0f;

			if (delta.Length2D() <= std::fmaxf(11.f, m_movement_data->m_pLocal->m_vecVelocity().Length2D() * Interfaces::m_pGlobalVars->interval_per_tick)) {
				float maxSpeed = m_movement_data->m_pLocal->GetMaxSpeed();
				static int playerSurfaceFrictionOffset = SDK::Memory::FindInDataMap(m_movement_data->m_pLocal->GetPredDescMap(), XorStr("m_surfaceFriction"));
				float playerSurfaceFriction = *(float*)(uintptr_t(m_movement_data->m_pLocal) + playerSurfaceFrictionOffset);
				float max_accelspeed = g_Vars.sv_accelerate->GetFloat() * Interfaces::m_pGlobalVars->interval_per_tick * maxSpeed * playerSurfaceFriction;

				m_movement_data->m_bStopPlayer = int(m_movement_data->m_pLocal->m_vecVelocity().Length2D() / max_accelspeed) - 1;
				m_movement_data->m_bMinimalSpeed = false;

				peek = true;
				g_Vars.globals.WasShootingInPeek = false;
			}
		}
		else {
			g_Vars.globals.WasShootingInPeek = false;
		}

		if (peek) {
			if (m_movement_data->m_bStopPlayer && AdjustVelocity()) {
				m_movement_data->m_bStoppedTick = true;
			}
		}

		if (!m_movement_data->m_bStoppedTick) {
			C_WeaponCSBaseGun* Weapon = (C_WeaponCSBaseGun*)m_movement_data->m_pLocal->m_hActiveWeapon().Get();
			if (Weapon) {
				auto weaponInfo = Weapon->GetCSWeaponData();
				if (g_Vars.misc.slow_walk && g_Vars.misc.slow_walk_bind.enabled) {
					if (Weapon && weaponInfo.IsValid())
						SlowWalk(cmd);
				}
				else if (g_Vars.misc.accurate_walk && m_movement_data->m_pCmd->buttons & IN_SPEED) {
					if (Weapon && weaponInfo.IsValid())
						SlowWalk(cmd);
				}
			}
		}

		m_movement_data->m_bStopPlayer--;
		m_movement_data->m_bMinimalSpeed = false;
		if (m_movement_data->m_bStopPlayer < 0)
			m_movement_data->m_bStopPlayer = 0;

		m_movement_data->m_pCmd->viewangles.Normalize();

		RotateMovement(m_movement_data->m_pCmd, m_movement_data->m_angMovementAngle, m_movement_data->m_pCmd->viewangles);
	}

	void C_Movement::InPrediction() {
		if (!m_movement_data->m_pLocal || m_movement_data->m_pLocal->IsDead() || !m_movement_data->m_pSendPacket)
			return;

		if (g_Vars.misc.edgejump && g_Vars.misc.edgejump_bind.enabled &&
			Engine::Prediction::Instance().GetFlags() & FL_ONGROUND && !(m_movement_data->m_pLocal->m_fFlags() & FL_ONGROUND)) {
			m_movement_data->m_pCmd->buttons |= IN_JUMP;
		}

		auto animState = m_movement_data->m_pLocal->m_PlayerAnimState();
		if (!animState)
			return;

		m_movement_data->m_pLocal->SetAbsAngles(QAngle(0.f, animState->m_flAbsRotation, 0.f));
		m_movement_data->m_pLocal->InvalidateBoneCache();

		const float PitchPosBackup = *(float*)(uintptr_t(m_movement_data->m_pLocal) + Engine::Displacement.DT_BaseAnimating.m_flPoseParameter + 48);

		*(float*)(uintptr_t(m_movement_data->m_pLocal) + Engine::Displacement.DT_BaseAnimating.m_flPoseParameter + 48) = 0.5f;

		g_BoneSetup.BuildBones(m_movement_data->m_pLocal, BONE_USED_BY_ANYTHING, BoneSetupFlags::None);

		g_Vars.globals.m_vecFixedEyePosition = m_movement_data->m_pLocal->GetEyePosition();

		*(float*)(uintptr_t(m_movement_data->m_pLocal) + Engine::Displacement.DT_BaseAnimating.m_flPoseParameter + 48) = PitchPosBackup;

		bool bDontFakelag = g_Vars.misc.move_exploit_key.enabled && g_Vars.misc.move_exploit && g_Vars.globals.bMoveExploiting;

		MoveExploit();

		if (g_Vars.fakelag.enabled) {
			if (!bDontFakelag)
				Interfaces::FakeLag::Get()->Main(m_movement_data->m_pSendPacket, m_movement_data->m_pCmd);
		}

		g_Vars.globals.bInRagebot = Interfaces::Ragebot::Get()->Run(m_movement_data->m_pCmd, m_movement_data->m_pLocal, m_movement_data->m_pSendPacket);


		// don't lag when shooting, this way events are instant
		if (!bDontFakelag)
			if (g_Vars.globals.m_bOldShot) {
				*m_movement_data->m_pSendPacket = true;
			}

		Interfaces::AntiAimbot::Get()->Main(m_movement_data->m_pSendPacket, m_movement_data->m_pFinalPacket, m_movement_data->m_pCmd, g_Vars.globals.bInRagebot);

		g_Vars.globals.m_StoredAngle = m_movement_data->m_pCmd->viewangles;

		Interfaces::KnifeBot::Get()->Main(m_movement_data->m_pCmd, m_movement_data->m_pSendPacket);
		Interfaces::ZeusBot::Get()->Main(m_movement_data->m_pCmd, m_movement_data->m_pSendPacket);
		g_Vars.globals.m_bInCreateMove = false;
	}

	void C_Movement::PostPrediction() {
		if (!m_movement_data->m_pLocal)
			return;

		if (!m_movement_data->m_pCmd.IsValid())
			return;

		//	m_movement_data->m_pCmd->forwardmove = Math::Clamp( m_movement_data->m_pCmd->forwardmove, -g_Vars.cl_forwardspeed->GetFloat( ), g_Vars.cl_forwardspeed->GetFloat( ) );
		//	m_movement_data->m_pCmd->sidemove = Math::Clamp( m_movement_data->m_pCmd->sidemove, -g_Vars.cl_sidespeed->GetFloat( ), g_Vars.cl_sidespeed->GetFloat( ) );
		//	m_movement_data->m_pCmd->upmove = Math::Clamp( m_movement_data->m_pCmd->upmove, -g_Vars.cl_upspeed->GetFloat( ), g_Vars.cl_upspeed->GetFloat( ) );
		m_movement_data->m_pCmd->viewangles.Normalize();
		m_movement_data->m_pCmd->viewangles.Clamp();

		float delta_x = std::remainderf(m_movement_data->m_pCmd->viewangles.x - m_movement_data->m_angPreviousAngles.x, 360.0f);
		float delta_y = std::remainderf(m_movement_data->m_pCmd->viewangles.y - m_movement_data->m_angPreviousAngles.y, 360.0f);

		if (delta_x != 0.0f) {
			float mouse_y = -((delta_x / g_Vars.m_pitch->GetFloat()) / g_Vars.sensitivity->GetFloat());
			short mousedy;
			if (mouse_y <= 32767.0f) {
				if (mouse_y >= -32768.0f) {
					if (mouse_y >= 1.0f || mouse_y < 0.0f) {
						if (mouse_y <= -1.0f || mouse_y > 0.0f)
							mousedy = static_cast<short>(mouse_y);
						else
							mousedy = -1;
					}
					else {
						mousedy = 1;
					}
				}
				else {
					mousedy = 0x8000u;
				}
			}
			else {
				mousedy = 0x7FFF;
			}

			m_movement_data->m_pCmd->mousedy = mousedy;
		}

		if (delta_y != 0.0f) {
			float mouse_x = -((delta_y / g_Vars.m_yaw->GetFloat()) / g_Vars.sensitivity->GetFloat());
			short mousedx;
			if (mouse_x <= 32767.0f) {
				if (mouse_x >= -32768.0f) {
					if (mouse_x >= 1.0f || mouse_x < 0.0f) {
						if (mouse_x <= -1.0f || mouse_x > 0.0f)
							mousedx = static_cast<short>(mouse_x);
						else
							mousedx = -1;
					}
					else {
						mousedx = 1;
					}
				}
				else {
					mousedx = 0x8000u;
				}
			}
			else {
				mousedx = 0x7FFF;
			}

			m_movement_data->m_pCmd->mousedx = mousedx;
		}

		RotateMovement(m_movement_data->m_pCmd, m_movement_data->m_angRenderViewangles, m_movement_data->m_pCmd->viewangles);

		m_movement_data->m_angPreviousAngles = m_movement_data->m_pCmd->viewangles;
	}

	bool C_Movement::AdjustVelocity() {


		bool m_between_shots = true;
		bool m_force_accuracy = false;

		auto pLocal = C_CSPlayer::GetLocalPlayer();
		C_WeaponCSBaseGun* Weapon = (C_WeaponCSBaseGun*)m_movement_data->m_pLocal->m_hActiveWeapon().Get();
		auto weaponInfo = Weapon->GetCSWeaponData();
		auto m_weapon_id = Weapon->m_iItemDefinitionIndex();
		bool not_auto = m_weapon_id == WEAPON_REVOLVER || m_weapon_id == WEAPON_AWP || m_weapon_id == WEAPON_SSG08 || m_weapon_id == WEAPON_ZEUS || weaponInfo->m_iWeaponType == WEAPONTYPE_SHOTGUN;


		if (g_Vars.globals.Fakewalking)
			return true;

		if ((!m_between_shots && !pLocal->CanShoot()) || (not_auto && !pLocal->CanShoot()))
			return false;

		if (!(pLocal->m_fFlags() & FL_ONGROUND) || (pLocal->m_fFlags() & IN_JUMP))
			return false;

		if ((pLocal->m_fFlags() & FL_ONGROUND))
			InstantStop();

		return false;


		QAngle angle;
		Math::VectorAngles(pLocal->m_vecVelocity(), angle);

		float speed = pLocal->m_vecVelocity().Length();

		angle.y = m_movement_data->m_pCmd->viewangles.y - angle.y;

		Vector direction;
		Math::AngleVectors(angle, &direction);

		Vector stop = direction * -speed;
		if ((pLocal->m_fFlags() & FL_ONGROUND)) {
			if (g_Vars.globals.m_speed > 10.f) {
				m_movement_data->m_pCmd->forwardmove = stop.x;
				m_movement_data->m_pCmd->sidemove = stop.y * 5;
			}
			else {
				m_movement_data->m_pCmd->forwardmove = 0.f;
				m_movement_data->m_pCmd->sidemove = 0.f;
			}
		}

		/*

		float percentage = 0.22;


		if (not_auto || !m_between_shots) {

			QAngle angle;
			Math::VectorAngles(pLocal->m_vecVelocity(), angle);

			float speed = pLocal->m_vecVelocity().Length2D();

			angle.y = m_movement_data->m_pCmd->viewangles.y - angle.y;

			Vector direction;
			Math::AngleVectors(angle, &direction);

			Vector stop = direction * -speed;

			if (g_Vars.globals.m_speed > 0.1f) {
				m_movement_data->m_pCmd->forwardmove = stop.x;
				m_movement_data->m_pCmd->sidemove = stop.y;
			}
			else {
				m_movement_data->m_pCmd->forwardmove = 0.f;
				m_movement_data->m_pCmd->sidemove = 0.f;
			}

			return true;
		}


		float v4 = percentage * (pLocal->m_bIsScoped() ? weaponInfo->m_flMaxSpeed2 : weaponInfo->m_flMaxSpeed);

		if (g_Vars.globals.m_vecUnpredictedVel.Length() < v4) {

			float squirt2 = std::sqrtf((m_movement_data->m_pCmd->forwardmove * m_movement_data->m_pCmd->forwardmove) + (m_movement_data->m_pCmd->sidemove * m_movement_data->m_pCmd->sidemove));

			float cock1 = m_movement_data->m_pCmd->forwardmove / squirt2;
			float cock2 = m_movement_data->m_pCmd->sidemove / squirt2;

			auto Velocity = pLocal->m_vecVelocity().Length();

			if (v4 + 1.0 <= Velocity) {
				m_movement_data->m_pCmd->forwardmove = 0;
				m_movement_data->m_pCmd->sidemove = 0;
			}
			else {
				m_movement_data->m_pCmd->forwardmove = cock1 * v4;
				m_movement_data->m_pCmd->sidemove = cock2 * v4;
			}
		}
		else
		{
			QAngle angle;
			Math::VectorAngles(pLocal->m_vecVelocity(), angle);

			float speed = pLocal->m_vecVelocity().Length();

			angle.y = m_movement_data->m_pCmd->viewangles.y - angle.y;

			Vector direction;
			Math::AngleVectors(angle, &direction);

			Vector stop = direction * -speed;

			m_movement_data->m_pCmd->forwardmove = stop.x;
			m_movement_data->m_pCmd->sidemove = stop.y;

			// onetap
			if (m_force_accuracy)
				return false;
		}

		return true;*/
	}

	void C_Movement::InstantStop(CUserCmd* cmd) {
		float maxSpeed = m_movement_data->m_pLocal->GetMaxSpeed();
		Vector velocity = m_movement_data->m_pLocal->m_vecVelocity();
		velocity.z = 0.0f;

		CUserCmd* ucmd = cmd ? cmd : m_movement_data->m_pCmd.Xor();

		float speed = velocity.Length2D();


		auto pLocal = C_CSPlayer::GetLocalPlayer();
		C_WeaponCSBaseGun* Weapon = (C_WeaponCSBaseGun*)m_movement_data->m_pLocal->m_hActiveWeapon().Get();
		auto weaponInfo = Weapon->GetCSWeaponData();
		auto m_weapon_id = Weapon->m_iItemDefinitionIndex();
		bool not_auto = m_weapon_id == WEAPON_REVOLVER || m_weapon_id == WEAPON_AWP || m_weapon_id == WEAPON_SSG08 || m_weapon_id == WEAPON_ZEUS || weaponInfo->m_iWeaponType == WEAPONTYPE_SHOTGUN;


		static int playerSurfaceFrictionOffset = SDK::Memory::FindInDataMap(m_movement_data->m_pLocal->GetPredDescMap(), XorStr("m_surfaceFriction"));
		float playerSurfaceFriction = *(float*)(uintptr_t(m_movement_data->m_pLocal) + playerSurfaceFrictionOffset);
		float max_accelspeed = g_Vars.sv_accelerate->GetFloat() * Interfaces::m_pGlobalVars->interval_per_tick * maxSpeed * playerSurfaceFriction;

		float shit_stopithinkmb = -1.f;

		if (!not_auto && !g_Vars.globals.Fakewalking) {
			if (speed < maxSpeed * 0.29)
				return;
		}
		else if (velocity.Length() < 10.f)
			return;



		if (speed - max_accelspeed <= -1.f) {
			m_movement_data->m_bStopPlayer = 0;
			ucmd->forwardmove = speed / max_accelspeed;
		}
		else {
			ucmd->forwardmove = g_Vars.cl_forwardspeed->GetFloat();
		}

		ucmd->sidemove = 0.0f;

		QAngle move_dir = m_movement_data->m_angMovementAngle;

		float direction = atan2(velocity.y, velocity.x);
		move_dir.yaw = std::remainderf(ToDegrees(direction) + 180.0f, 360.0f);
		RotateMovement(ucmd, move_dir, ucmd->viewangles);

	}

	bool C_Movement::AutoStopInternal() {
		return false;
	}

	void C_Movement::AutoJump() {
		if (m_movement_data->m_pLocal->m_MoveType() == MOVETYPE_LADDER || m_movement_data->m_pLocal->m_MoveType() == MOVETYPE_NOCLIP)
			return;

		if (g_Vars.rage.enabled) {
			if (!(m_movement_data->m_pLocal->m_fFlags() & FL_ONGROUND)) {
				m_movement_data->m_pCmd->buttons &= ~IN_JUMP;
			}
			return;
		}

		if (!m_movement_data->jumped_last_tick && m_movement_data->should_fake_jump) {
			m_movement_data->should_fake_jump = false;
			m_movement_data->m_pCmd->buttons |= IN_JUMP;
		}
		else if (m_movement_data->m_pCmd->buttons & IN_JUMP) {
			if (m_movement_data->m_pLocal->m_fFlags() & FL_ONGROUND) {
				m_movement_data->jumped_last_tick = true;
				m_movement_data->should_fake_jump = true;
			}
			else {
				m_movement_data->m_pCmd->buttons &= ~IN_JUMP;
				m_movement_data->jumped_last_tick = false;
			}
		}
		else {
			m_movement_data->jumped_last_tick = false;
			m_movement_data->should_fake_jump = false;
		}
	}

	void C_Movement::AutoStrafe() {
		if ((m_movement_data->m_pLocal->m_fFlags() & FL_ONGROUND) && !(m_movement_data->m_pCmd->buttons & IN_JUMP))
			return;

		if (m_movement_data->m_pLocal->m_MoveType() != MOVETYPE_WALK || m_movement_data->m_pLocal->m_MoveType() == MOVETYPE_NOCLIP || m_movement_data->m_pLocal->m_MoveType() == MOVETYPE_LADDER)
			return;

		if (g_Vars.misc.slow_walk && g_Vars.misc.slow_walk_bind.enabled)
			return;

		static auto side = 1.0f;
		side = -side;

		auto velocity = m_movement_data->m_pLocal->m_vecVelocity();
		velocity.z = 0.0f;

		auto speed = velocity.Length2D();
		auto ideal_strafe = Math::Clamp(ToDegrees(atan(15.f / speed)), 0.0f, 90.0f);

		if (g_Vars.misc.autostrafer_wasd && (m_movement_data->m_pCmd->forwardmove != 0.0f || m_movement_data->m_pCmd->sidemove != 0.0f)) {
			// took this idea from exon, thank u !!!!
			enum EDirections {
				FORWARDS = 0,
				BACKWARDS = 180,
				LEFT = 90,
				RIGHT = -90,
				BACK_LEFT = 135,
				BACK_RIGHT = -135
			};

			float wish_dir{ };

			// get our key presses.
			bool holding_w = m_movement_data->m_pCmd->buttons & IN_FORWARD;
			bool holding_a = m_movement_data->m_pCmd->buttons & IN_MOVELEFT;
			bool holding_s = m_movement_data->m_pCmd->buttons & IN_BACK;
			bool holding_d = m_movement_data->m_pCmd->buttons & IN_MOVERIGHT;

			// move in the appropriate direction.
			if (holding_w) {
				//	forward left
				if (holding_a) {
					wish_dir += (EDirections::LEFT / 2);
				}
				//	forward right
				else if (holding_d) {
					wish_dir += (EDirections::RIGHT / 2);
				}
				//	forward
				else {
					wish_dir += EDirections::FORWARDS;
				}
			}
			else if (holding_s) {
				//	back left
				if (holding_a) {
					wish_dir += EDirections::BACK_LEFT;
				}
				//	back right
				else if (holding_d) {
					wish_dir += EDirections::BACK_RIGHT;
				}
				//	back
				else {
					wish_dir += EDirections::BACKWARDS;
				}

				// cancel out any forwardmove values.
				m_movement_data->m_pCmd->forwardmove = 0.f;
			}
			else if (holding_a) {
				//	left
				wish_dir += EDirections::LEFT;
			}
			else if (holding_d) {
				//	right
				wish_dir += EDirections::RIGHT;
			}

			m_movement_data->m_angMovementAngle.yaw += std::remainderf(wish_dir, 360.f);

			m_movement_data->m_pCmd->sidemove = 0.f;
		}

		// cancel out any forwardmove values.
		m_movement_data->m_pCmd->forwardmove = 0.f;

		auto yaw_delta = std::remainderf(m_movement_data->m_angMovementAngle.yaw - m_movement_data->m_flOldYaw, 360.0f);
		auto abs_angle_delta = abs(yaw_delta);
		m_movement_data->m_flOldYaw = m_movement_data->m_angMovementAngle.yaw;

		if (abs_angle_delta <= ideal_strafe || abs_angle_delta >= 30.0f) {
			auto velocity_direction = velocity.ToEulerAngles();
			auto velocity_delta = std::remainderf(m_movement_data->m_angMovementAngle.yaw - velocity_direction.yaw, 360.0f);
			if (velocity_delta <= ideal_strafe || speed <= 15.0f) {
				if (-(ideal_strafe) <= velocity_delta || speed <= 15.0f) {
					m_movement_data->m_angMovementAngle.yaw += side * ideal_strafe;
					m_movement_data->m_pCmd->sidemove = 450 * side;
				}
				else {
					m_movement_data->m_angMovementAngle.yaw = velocity_direction.yaw - ideal_strafe;
					m_movement_data->m_pCmd->sidemove = 450;
				}
			}
			else {
				m_movement_data->m_angMovementAngle.yaw = velocity_direction.yaw + ideal_strafe;
				m_movement_data->m_pCmd->sidemove = -450;
			}
		}
		else if (yaw_delta > 0.0f) {
			m_movement_data->m_pCmd->sidemove = -450;
		}
		else if (yaw_delta < 0.0f) {
			m_movement_data->m_pCmd->sidemove = 450;
		}

		m_movement_data->m_angMovementAngle.Normalize();
	}

	void C_Movement::SlowWalk(Encrypted_t<CUserCmd> cmd) 
	{
		Vector velocity{ m_movement_data->m_pLocal->m_vecVelocity() };
		int    ticks{ }, max{ 14 };

		auto pLocal = C_CSPlayer::GetLocalPlayer();

		if (!pLocal || pLocal->IsDead())
			return;

		if (!(g_Vars.misc.slow_walk && g_Vars.misc.slow_walk_bind.enabled))
			return;

		static auto sv_friction = Interfaces::m_pCvar->FindVar(XorStr("sv_friction"));
		static auto sv_stopspeed = Interfaces::m_pCvar->FindVar(XorStr("sv_stopspeed"));
		static int playerSurfaceFrictionOffset = SDK::Memory::FindInDataMap(m_movement_data->m_pLocal->GetPredDescMap(), XorStr("m_surfaceFriction"));
		float playerSurfaceFriction = *(float*)(uintptr_t(m_movement_data->m_pLocal) + playerSurfaceFrictionOffset);

		// calculate friction.
		float friction = sv_friction->GetFloat() * playerSurfaceFriction;

		int m_max_lag = (pLocal->m_fFlags() & FL_ONGROUND) ? 14 : 13;

		for (; ticks < m_max_lag; ++ticks) {
			// calculate speed.
			float speed = velocity.Length();

			// if too slow return.
			if (speed <= 0.1f)
				break;

			// bleed off some speed, but if we have less than the bleed, threshold, bleed the threshold amount.
			float control = std::max(speed, sv_stopspeed->GetFloat());

			// calculate the drop amount.
			float drop = control * friction * Interfaces::m_pGlobalVars->interval_per_tick;

			// scale the velocity.
			float newspeed = std::max(0.f, speed - drop);

			if (newspeed != speed) {
				// determine proportion of old speed we are using.
				newspeed /= speed;

				// adjust velocity according to proportion.
				velocity *= newspeed;
			}
		}

		// zero forwardmove and sidemove.
		if (ticks > ((max - 1) - Interfaces::m_pClientState->m_nChokedCommands()) || !Interfaces::m_pClientState->m_nChokedCommands()) {
			cmd->forwardmove = cmd->sidemove = 0.f;
		}
	}

	void C_Movement::ThirdPerson() {
		if (!m_movement_data->m_pLocal)
			return;

		auto local = C_CSPlayer::GetLocalPlayer();
		if (!local || local != m_movement_data->m_pLocal)
			return;

		// for whatever reason overrideview also gets called from the main menu.
		if (!Interfaces::m_pEngine->IsInGame())
			return;

		// check if we have a local player and he is alive.
		bool alive = !local->IsDead();

		static bool bThirdPerson = false;

		if (alive) {
			C_WeaponCSBaseGun* Weapon = (C_WeaponCSBaseGun*)m_movement_data->m_pLocal->m_hActiveWeapon().Get();

			if (!Weapon)
				return;

			auto weaponInfo = Weapon->GetCSWeaponData();
			if (!weaponInfo.IsValid())
				return;

			if (weaponInfo->m_iWeaponType == WEAPONTYPE_GRENADE && g_Vars.misc.third_person_on_grenade) {
				Interfaces::m_pInput->CAM_ToFirstPerson();
				Interfaces::m_pInput->m_fCameraInThirdPerson = false;
				return;
			}
		}

		// camera should be in thirdperson.
		if (g_Vars.misc.third_person && g_Vars.misc.third_person_bind.enabled) {

			// if alive and not in thirdperson already switch to thirdperson.
			if (alive && !Interfaces::m_pInput->CAM_IsThirdPerson()) {
				Interfaces::m_pInput->CAM_ToThirdPerson();
			}
			// fix the dogshit spectate bug.
			if (!alive)
			{
				g_Vars.misc.third_person_bind.enabled = false;
				Interfaces::m_pInput->m_fCameraInThirdPerson = false;
			}
			// if dead and spectating in thirdperson switch to firstperson.
			else if (m_movement_data->m_pLocal->m_iObserverMode() == 4) {

				// if in thirdperson, switch to firstperson.
				// we need to disable thirdperson to spectate properly.
				if (Interfaces::m_pInput->CAM_IsThirdPerson()) {
					Interfaces::m_pInput->CAM_ToFirstPerson();
				}

				m_movement_data->m_pLocal->m_iObserverMode() = 5;
			}
		}
		// camera should be in firstperson.
		else {
			Interfaces::m_pInput->CAM_ToFirstPerson();
		}

		// if after all of this we are still in thirdperson.
		if (Interfaces::m_pInput->CAM_IsThirdPerson()) {
			// get camera angles.
			QAngle offset;
			Interfaces::m_pEngine->GetViewAngles(offset);

			// get our viewangle's forward directional vector.
			Vector forward;
			Math::AngleVectors(offset, forward);

			offset.z = g_Vars.misc.third_person_dist;

			Vector offsetd = m_movement_data->m_pLocal->m_vecViewOffset();

			// start pos.
			Vector origin = m_movement_data->m_pLocal->GetAbsOrigin() + offsetd;

			// setup trace filter and trace.
			CTraceFilterWorldAndPropsOnly filter;
			CGameTrace tr;

			Interfaces::m_pEngineTrace->TraceRay(
				Ray_t(origin, origin - (forward * offset.z), { -16.f, -16.f, -16.f }, { 16.f, 16.f, 16.f }),
				MASK_NPCWORLDSTATIC,
				(ITraceFilter*)&filter,
				&tr
			);

			// adapt distance to travel time.
			Math::Clamp(tr.fraction, 0.f, 1.f);
			offset.z *= tr.fraction;

			// override camera angles.
			Interfaces::m_pInput->m_vecCameraOffset = { offset.x, offset.y, offset.z };

			m_movement_data->m_pLocal->UpdateVisibilityAllEntities();
		}
	}

	void C_Movement::AutoStop(int ticks) {
		m_movement_data->m_bStopPlayer = ticks;
	}

	bool C_Movement::GetStopState() {
		return m_movement_data->m_bStopOk;
	}

	bool C_Movement::CreateMoveRecursion() {
		if (!m_movement_data->m_bInRecursion || m_movement_data->m_iRecursionTicks <= 0) {
			m_movement_data->m_bInRecursion = false;
			m_movement_data->m_iRecursionTicks = 0;
			return false;
		}

		m_movement_data->m_iRecursionTicks--;
		return true;
	}

	void C_Movement::MoveExploit() {
		static bool bDisallow = false;
		static bool bSaveOrigin = true;

		if (!g_Vars.misc.move_exploit) {
			g_Vars.globals.vecExploitOrigin.Init();
			bDisallow = false;
			bSaveOrigin = true;
			g_Vars.globals.bMoveExploiting = false;
			return;
		}

		if (!g_Vars.misc.move_exploit_key.enabled) {
			g_Vars.globals.vecExploitOrigin.Init();
			bDisallow = false;
			bSaveOrigin = true;
			g_Vars.globals.bMoveExploiting = false;
			return;
		}

		if (m_movement_data->m_pLocal->m_vecVelocity().Length2D() < m_movement_data->m_pLocal->GetMaxSpeed() - 20.f) {
			g_Vars.globals.vecExploitOrigin.Init();
			g_Vars.globals.bMoveExploiting = false;
			bSaveOrigin = true;
			return;
		}

		if (Interfaces::m_pClientState->m_nChokedCommands() < 100) {
			if (!bDisallow) {
				*m_movement_data->m_pSendPacket = false;
				g_Vars.globals.bMoveExploiting = true;
			}

			if (bSaveOrigin) {
				g_Vars.globals.vecExploitOrigin = m_movement_data->m_pLocal->GetEyePosition() + (m_movement_data->m_pLocal->m_vecVelocity() * 100 * Interfaces::m_pGlobalVars->interval_per_tick);
				bSaveOrigin = false;
			}
		}
		else {
			bSaveOrigin = true;
			g_Vars.globals.bMoveExploiting = false;
		}
	}
}