#include "AnimationSystem.hpp"
#include "../../SDK/displacement.hpp"
#include "../../Utils/Math.h"
#include "../Game/SetupBones.hpp"
#include "../../Utils/Threading/threading.h"
#include "LagCompensation.hpp"
#include "Resolver.hpp"

#define MT_SETUP_BONES

namespace Engine
{
	struct SimulationRestore {
		int m_fFlags;
		float m_flDuckAmount;
		float m_flFeetCycle;
		float m_flFeetYawRate;
		QAngle m_angEyeAngles;
		Vector m_vecOrigin;

		void Setup(C_CSPlayer* player) {
			m_fFlags = player->m_fFlags();
			m_flDuckAmount = player->m_flDuckAmount();
			m_vecOrigin = player->m_vecOrigin();
			m_angEyeAngles = player->m_angEyeAngles();

			auto animState = player->m_PlayerAnimState();
			m_flFeetCycle = animState->m_flFeetCycle;
			m_flFeetYawRate = animState->m_flFeetYawRate;
		}

		void Apply(C_CSPlayer* player) const {
			player->m_fFlags() = m_fFlags;
			player->m_flDuckAmount() = m_flDuckAmount;
			player->m_vecOrigin() = m_vecOrigin;
			player->m_angEyeAngles() = m_angEyeAngles;

			auto animState = player->m_PlayerAnimState();
			animState->m_flFeetCycle = m_flFeetCycle;
			animState->m_flFeetYawRate = m_flFeetYawRate;
		}
	};

	struct AnimationBackup {

		CCSGOPlayerAnimState anim_state;
		C_AnimationLayer layers[13];
		float pose_params[19];

		AnimationBackup() {

		}

		void Apply(C_CSPlayer* player) const;
		void Setup(C_CSPlayer* player);
	};

	void AnimationBackup::Apply(C_CSPlayer* player) const {
		*player->m_PlayerAnimState() = this->anim_state;
		std::memcpy(player->m_AnimOverlay().m_Memory.m_pMemory, layers, sizeof(layers));
		std::memcpy(player->m_flPoseParameter(), pose_params, sizeof(pose_params));
	}

	void AnimationBackup::Setup(C_CSPlayer* player) {
		this->anim_state = *player->m_PlayerAnimState();
		std::memcpy(layers, player->m_AnimOverlay().m_Memory.m_pMemory, sizeof(layers));
		std::memcpy(pose_params, player->m_flPoseParameter(), sizeof(pose_params));
	}

	inline void FixBonesRotations(C_CSPlayer* player, matrix3x4_t* bones) {
		// copypasted from supremacy/fatality, no difference imo
		// also seen that in aimware multipoints, but was lazy to paste, kek
		auto studio_hdr = player->m_pStudioHdr();
		if (studio_hdr) {
			auto hdr = *(studiohdr_t**)studio_hdr;
			if (hdr) {
				auto hitboxSet = hdr->pHitboxSet(player->m_nHitboxSet());
				for (int i = 0; i < hitboxSet->numhitboxes; i++) {
					auto hitbox = hitboxSet->pHitbox(i);
					if (hitbox->m_angAngles.IsZero())
						continue;

					matrix3x4_t hitboxTransform;
					hitboxTransform.AngleMatrix(hitbox->m_angAngles);
					bones[hitbox->bone] = bones[hitbox->bone].ConcatTransforms(hitboxTransform);
				}
			}
		}
	}

	class C_AnimationSystem : public AnimationSystem {
	public:
		virtual void CollectData();
		virtual void Update();

		virtual C_AnimationData* GetAnimationData(int index) {
			if (m_AnimatedEntities.count(index) < 1)
				return nullptr;

			return &m_AnimatedEntities[index];
		}

		std::map<int, C_AnimationData> m_AnimatedEntities = { };

		C_AnimationSystem() { };
		virtual ~C_AnimationSystem() { };
	};

	Encrypted_t<AnimationSystem> AnimationSystem::Get() {
		static C_AnimationSystem instance;
		return &instance;
	}

	void C_AnimationSystem::CollectData() {
		if (!Interfaces::m_pEngine->IsInGame() || !Interfaces::m_pEngine->GetNetChannelInfo()) {
			this->m_AnimatedEntities.clear();
			return;
		}

		auto local = C_CSPlayer::GetLocalPlayer();
		if (!local || !g_Vars.globals.HackIsReady)
			return;

		for (int i = 1; i <= Interfaces::m_pGlobalVars->maxClients; ++i) {
			auto player = C_CSPlayer::GetPlayerByIndex(i);
			if (!player || player == local)
				continue;

			player_info_t player_info;
			if (!Interfaces::m_pEngine->GetPlayerInfo(player->m_entIndex, &player_info)) {
				continue;
			}

			this->m_AnimatedEntities[i].Collect(player);
		}
	}

	void C_AnimationSystem::Update() {
		if (!Interfaces::m_pEngine->IsInGame() || !Interfaces::m_pEngine->GetNetChannelInfo()) {
			this->m_AnimatedEntities.clear();
			return;
		}

		auto local = C_CSPlayer::GetLocalPlayer();
		if (!local || !g_Vars.globals.HackIsReady)
			return;

		for (auto& [key, value] : this->m_AnimatedEntities) {
			auto entity = C_CSPlayer::GetPlayerByIndex(key);
			if (!entity)
				continue;

			auto curtime = Interfaces::m_pGlobalVars->curtime;
			auto frametime = Interfaces::m_pGlobalVars->frametime;

			Interfaces::m_pGlobalVars->curtime = entity->m_flOldSimulationTime() + Interfaces::m_pGlobalVars->interval_per_tick;
			Interfaces::m_pGlobalVars->frametime = Interfaces::m_pGlobalVars->interval_per_tick;

			if (value.m_bUpdated)
				value.Update();

			Interfaces::m_pGlobalVars->curtime = curtime;
			Interfaces::m_pGlobalVars->frametime = frametime;

			value.m_bUpdated = false;
		}

	}

	void C_AnimationData::Update() {
		if (!this->player || this->m_AnimationRecord.size() < 1)
			return;

		C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer();
		if (!pLocal)
			return;

		auto pAnimationRecord = Encrypted_t<Engine::C_AnimationRecord>(&this->m_AnimationRecord.front());
		Encrypted_t<Engine::C_AnimationRecord> pPreviousAnimationRecord(nullptr);
		if (this->m_AnimationRecord.size() > 1) {
			pPreviousAnimationRecord = &this->m_AnimationRecord.at(1);
		}

		this->player->m_vecVelocity() = pAnimationRecord->m_vecAnimationVelocity;

		auto weapon = (C_BaseAttributableItem*)player->m_hActiveWeapon().Get();
		auto weaponWorldModel = weapon ? (C_CSPlayer*)(weapon)->m_hWeaponWorldModel().Get() : nullptr;

		auto animState = player->m_PlayerAnimState();
		if (!animState)
			return;

		// simulate animations
		SimulateAnimations(pAnimationRecord, pPreviousAnimationRecord);

		// update layers
		std::memcpy(player->m_AnimOverlay().Base(), pAnimationRecord->m_serverAnimOverlays, 13 * sizeof(C_AnimationLayer));

		// generate aimbot matrix
		g_BoneSetup.SetupBonesRebuild(player, m_Bones, 128, BONE_USED_BY_ANYTHING & ~BONE_USED_BY_BONE_MERGE, player->m_flSimulationTime(), BoneSetupFlags::UseCustomOutput);

		// generate visual matrix
		g_BoneSetup.SetupBonesRebuild(player, nullptr, 128, 0x7FF00, player->m_flSimulationTime(), BoneSetupFlags::ForceInvalidateBoneCache | BoneSetupFlags::AttachmentHelper);

		this->m_vecSimulationData.clear();
	}

	void C_AnimationData::Collect(C_CSPlayer* player) {
		if (player->IsDead())
			player = nullptr;

		auto pThis = Encrypted_t<C_AnimationData>(this);

		if (pThis->player != player) {
			pThis->m_flSpawnTime = 0.0f;
			pThis->m_flSimulationTime = 0.0f;
			pThis->m_flOldSimulationTime = 0.0f;
			pThis->m_iCurrentTickCount = 0;
			pThis->m_iOldTickCount = 0;
			pThis->m_iTicksAfterDormancy = 0;
			pThis->m_vecSimulationData.clear();
			pThis->m_AnimationRecord.clear();
			pThis->m_bIsDormant = pThis->m_bBonesCalculated = false;
			pThis->player = player;
			pThis->m_bIsAlive = false;
		}

		if (!player)
			return;

		pThis->m_bIsAlive = true;
		pThis->m_flOldSimulationTime = pThis->m_flSimulationTime;
		pThis->m_flSimulationTime = pThis->player->m_flSimulationTime();

		if (pThis->m_flSimulationTime == 0.0f || pThis->player->IsDormant()) {
			pThis->m_bIsDormant = true;
			Engine::g_ResolverData[player->EntIndex()].m_bWentDormant = true;
			Engine::g_ResolverData[player->EntIndex()].m_sMoveData.m_flSimulationTime = 0.f;
			Engine::g_ResolverData[player->EntIndex()].m_vecSavedOrigin = pThis->m_vecOrigin;
			return;
		}

		if (pThis->m_flOldSimulationTime == pThis->m_flSimulationTime) {
			return;
		}

		if (pThis->m_bIsDormant) {
			pThis->m_iTicksAfterDormancy = 0;
			pThis->m_AnimationRecord.clear();

			Engine::g_ResolverData[player->EntIndex()].m_sMoveData.m_flSimulationTime = 0.f;
			Engine::g_ResolverData[player->EntIndex()].m_bWentDormant = true;
		}

		pThis->ent_index = player->m_entIndex;

		pThis->m_bUpdated = true;
		pThis->m_bIsDormant = false;

		pThis->m_iOldTickCount = pThis->m_iCurrentTickCount;
		pThis->m_iCurrentTickCount = Interfaces::m_pGlobalVars->tickcount;

		if (pThis->m_flSpawnTime != pThis->player->m_flSpawnTime()) {
			auto animState = pThis->player->m_PlayerAnimState();
			if (animState) {
				animState->m_Player = pThis->player;
				animState->Reset();
			}

			pThis->m_flSpawnTime = pThis->player->m_flSpawnTime();
		}

		int nTickRate = int(1.0f / Interfaces::m_pGlobalVars->interval_per_tick);

		while (pThis->m_AnimationRecord.size() > 256) {
			pThis->m_AnimationRecord.pop_back();
		}

		pThis->m_iTicksAfterDormancy++;

		Encrypted_t<C_AnimationRecord> previous_record = nullptr;
		Encrypted_t<C_AnimationRecord> penultimate_record = nullptr;

		if (pThis->m_AnimationRecord.size() > 0) {
			previous_record = &pThis->m_AnimationRecord.front();
			if (pThis->m_AnimationRecord.size() > 1) {
				penultimate_record = &pThis->m_AnimationRecord.at(1);
			}
		}

		auto record = &pThis->m_AnimationRecord.emplace_front();

		pThis->m_vecOrigin = pThis->player->m_vecOrigin();

		record->m_vecOrigin = pThis->player->m_vecOrigin();
		record->m_angEyeAngles = pThis->player->m_angEyeAngles();
		record->m_flSimulationTime = pThis->m_flSimulationTime;
		record->m_flLowerBodyYawTarget = pThis->player->m_flLowerBodyYawTarget();

		auto weapon = (C_WeaponCSBaseGun*)(player->m_hActiveWeapon().Get());

		if (weapon) {
			auto weaponWorldModel = (C_CSPlayer*)((C_BaseAttributableItem*)weapon)->m_hWeaponWorldModel().Get();

			for (int i = 0; i < player->m_AnimOverlay().Count(); ++i) {
				player->m_AnimOverlay().Element(i).m_pOwner = player;
				player->m_AnimOverlay().Element(i).m_pStudioHdr = player->m_pStudioHdr();

				if (weaponWorldModel) {
					if (player->m_AnimOverlay().Element(i).m_nSequence < 2 || player->m_AnimOverlay().Element(i).m_flWeight <= 0.0f)
						continue;

					using UpdateDispatchLayer = void(__thiscall*)(void*, C_AnimationLayer*, CStudioHdr*, int);
					Memory::VCall< UpdateDispatchLayer >(player, 241)(player, &player->m_AnimOverlay().Element(i),
						weaponWorldModel->m_pStudioHdr(), player->m_AnimOverlay().Element(i).m_nSequence);
				}
			}
		}

		std::memcpy(record->m_serverAnimOverlays, pThis->player->m_AnimOverlay().Base(), sizeof(record->m_serverAnimOverlays));

		record->m_flFeetCycle = record->m_serverAnimOverlays[6].m_flCycle;
		record->m_flFeetYawRate = record->m_serverAnimOverlays[6].m_flWeight;

		record->m_fFlags = player->m_fFlags();
		record->m_flDuckAmount = player->m_flDuckAmount();

		record->m_bIsShooting = false;
		record->m_flShotTime = 0.0f;
		record->m_bFakeWalking = false;
		record->m_bFakeFlicking = false;

		if (previous_record.IsValid()) {
			record->m_flChokeTime = pThis->m_flSimulationTime - pThis->m_flOldSimulationTime;
			record->m_iChokeTicks = TIME_TO_TICKS(record->m_flChokeTime);
		}
		else {
			record->m_flChokeTime = Interfaces::m_pGlobalVars->interval_per_tick;
			record->m_iChokeTicks = 1;
		}

		if (!previous_record.IsValid()) {
			record->m_bIsInvalid = true;
			record->m_vecVelocity.Init();
			record->m_bIsShooting = false;
			record->m_bTeleportDistance = false;

			//auto animstate = player->m_PlayerAnimState( );
			//if( animstate )
			//	animstate->m_flAbsRotation = record->m_angEyeAngles.yaw;

			return;
		}
		/*auto flPreviousSimulationTime = previous_record->m_flSimulationTime;
		auto nTickcountDelta = pThis->m_iCurrentTickCount - pThis->m_iOldTickCount;
		auto nSimTicksDelta = record->m_iChokeTicks;
		auto nChokedTicksUnk = nSimTicksDelta;
		auto bShiftedTickbase = false;

		if (pThis->m_flOldSimulationTime > pThis->m_flSimulationTime) {
			record->m_bShiftingTickbase = true;
			record->m_iChokeTicks = nTickcountDelta;
			record->m_flChokeTime = TICKS_TO_TIME(record->m_iChokeTicks);
			flPreviousSimulationTime = record->m_flSimulationTime - record->m_flChokeTime;
			nChokedTicksUnk = nTickcountDelta;
			bShiftedTickbase = true;
		}

		if (bShiftedTickbase || abs(nSimTicksDelta - nTickcountDelta) <= 2) {
			if (nChokedTicksUnk) {
				if (nChokedTicksUnk != 1) {
					pThis->m_iTicksUnknown = 0;
				}
				else {
					pThis->m_iTicksUnknown++;
				}
			}
			else {
				record->m_iChokeTicks = 1;
				record->m_flChokeTime = Interfaces::m_pGlobalVars->interval_per_tick;

				flPreviousSimulationTime = record->m_flSimulationTime - Interfaces::m_pGlobalVars->interval_per_tick;

				pThis->m_iTicksUnknown++;
			}
		}
		*/

		if (weapon) {
			record->m_flShotTime = weapon->m_fLastShotTime();
			record->m_bIsShooting = TIME_TO_TICKS(weapon->m_fLastShotTime()) >= TIME_TO_TICKS(record->m_flSimulationTime) - record->m_iChokeTicks;
		}

		record->m_bIsInvalid = false;

		// fix velocity
		// https://github.com/VSES/SourceEngine2007/blob/master/se2007/game/client/c_baseplayer.cpp#L659
		if (record->m_iChokeTicks > 0 && record->m_iChokeTicks <= 16 && pThis->m_AnimationRecord.size() >= 2) {
			record->m_vecVelocity = (record->m_vecOrigin - previous_record->m_vecOrigin) * (1.f / record->m_flChokeTime);
		}

		// fix CGameMovement::FinishGravity
		if (!(player->m_fFlags() & FL_ONGROUND))
			record->m_vecVelocity.z -= TICKS_TO_TIME(g_Vars.sv_gravity->GetFloat());
		else
			record->m_vecVelocity.z = 0.0f;

		record->m_vecAnimationVelocity = record->m_vecVelocity;

		if (record->m_bFakeWalking) {
			record->m_vecAnimationVelocity.Init();
		}

		// delta in time..
		float time = record->m_flSimulationTime - previous_record->m_flSimulationTime;

		if (!record->m_bFakeWalking) {
			// fix the velocity till the moment of animation.
			Vector velo = record->m_vecVelocity - previous_record->m_vecVelocity;

			// accel per tick.
			Vector accel = (velo / time) * Interfaces::m_pGlobalVars->interval_per_tick;

			// set the anim velocity to the previous velocity.
			// and predict one tick ahead.
			record->m_vecAnimationVelocity = previous_record->m_vecVelocity + accel;
		}

		record->m_bTeleportDistance = record->m_vecOrigin.DistanceSquared(previous_record->m_vecOrigin) > 4096.0f;

		C_SimulationInfo& data = pThis->m_vecSimulationData.emplace_back();
		data.m_flTime = previous_record->m_flSimulationTime + Interfaces::m_pGlobalVars->interval_per_tick;
		data.m_flDuckAmount = record->m_flDuckAmount;
		data.m_flLowerBodyYawTarget = record->m_flLowerBodyYawTarget;
		data.m_vecOrigin = record->m_vecOrigin;
		data.m_vecVelocity = record->m_vecAnimationVelocity;
		data.bOnGround = record->m_fFlags & FL_ONGROUND;

		// lets check if they are choking ticks.
		if (record->m_iChokeTicks > 1) {
			// TODO: calculate jump time
			// calculate landing time
			float flLandTime = 0.0f;
			bool bJumped = false;
			bool bLandedOnServer = false;
			if (record->m_serverAnimOverlays[4].m_flCycle < 0.5f && (!(record->m_fFlags & FL_ONGROUND) || !(previous_record->m_fFlags & FL_ONGROUND))) {
				// note - VIO (violations btw);
				// well i guess when llama wrote v3, he was drunk or sum cuz this is incorrect. -> cuz he changed this in v4.
				// and alpha didn't realize this but i did, so its fine.
				// improper way to do this -> flLandTime = record->m_flSimulationTime - float( record->m_serverAnimOverlays[ 4 ].m_flPlaybackRate * record->m_serverAnimOverlays[ 4 ].m_flCycle );
				// we need to divide instead of multiply.
				flLandTime = record->m_flSimulationTime - float(record->m_serverAnimOverlays[4].m_flPlaybackRate / record->m_serverAnimOverlays[4].m_flCycle);
				bLandedOnServer = flLandTime >= previous_record->m_flSimulationTime;
			}

			bool bOnGround = record->m_fFlags & FL_ONGROUND;
			// jump_fall fix
			if (bLandedOnServer && !bJumped) {
				if (flLandTime <= data.m_flTime) {
					bJumped = true;
					bOnGround = true;
				}
				else {
					bOnGround = previous_record->m_fFlags & FL_ONGROUND;
				}
			}

			data.bOnGround = bOnGround;

			// detect fakewalking players
			if (record->m_vecVelocity.Length() > 0.1f
				&& record->m_iChokeTicks >= 13
				&& record->m_serverAnimOverlays[6].m_flWeight == 0.0f
				&& record->m_serverAnimOverlays[6].m_flPlaybackRate < 0.0001f
				&& record->m_serverAnimOverlays[12].m_flWeight > 0.0f
				&& (record->m_fFlags & FL_ONGROUND))
				record->m_bFakeWalking = true;

			if (record->m_vecVelocity.Length() > 0.1f && record->m_fFlags & FL_ONGROUND && record->m_serverAnimOverlays[6].m_flWeight == 0.0f && record->m_vecVelocity.Length() < 100.f)
				record->m_bFakeWalking = true;

			if (record->m_bFakeWalking)
				record->m_vecAnimationVelocity = record->m_vecVelocity = { 0.f, 0.f, 0.f };

			// detect fake flicking players
			if (record->m_vecVelocity.Length() < 18.f
				&& record->m_serverAnimOverlays[6].m_flWeight != 1.0f
				&& record->m_serverAnimOverlays[6].m_flWeight != 0.0f
				&& record->m_serverAnimOverlays[6].m_flWeight != previous_record->m_serverAnimOverlays[6].m_flWeight
				&& (record->m_fFlags & FL_ONGROUND))
				record->m_bFakeFlicking = true;

			if (record->m_bFakeFlicking)
				record->m_vecAnimationVelocity = record->m_vecVelocity = { 0.f, 0.f, 0.f };

		}
	}

	void C_AnimationData::SimulateAnimations(Encrypted_t<Engine::C_AnimationRecord> current, Encrypted_t<Engine::C_AnimationRecord> previous) {
		auto UpdateAnimations = [&](C_CSPlayer* player, float flTime) {
			auto curtime = Interfaces::m_pGlobalVars->curtime;
			auto frametime = Interfaces::m_pGlobalVars->frametime;

			// force to use correct abs origin and velocity ( no CalcAbsolutePosition and CalcAbsoluteVelocity calls )
			player->m_iEFlags() &= ~(EFL_DIRTY_ABSTRANSFORM | EFL_DIRTY_ABSVELOCITY);

			int ticks = TIME_TO_TICKS(flTime);

			// calculate animations based on ticks aka server frames instead of render frames
			Interfaces::m_pGlobalVars->curtime = flTime;
			Interfaces::m_pGlobalVars->frametime = Interfaces::m_pGlobalVars->interval_per_tick;

			auto animstate = player->m_PlayerAnimState();
			if (animstate && animstate->m_nLastFrame >= Interfaces::m_pGlobalVars->framecount)
				animstate->m_nLastFrame = Interfaces::m_pGlobalVars->framecount - 1;

			for (int i = 0; i < player->m_AnimOverlay().Count(); ++i) {
				player->m_AnimOverlay().Base()[i].m_pOwner = player;
				player->m_AnimOverlay().Base()[i].m_pStudioHdr = player->m_pStudioHdr();
			}

			static auto& EnableInvalidateBoneCache = **reinterpret_cast<bool**>(Memory::Scan(XorStr("client.dll"), XorStr("C6 05 ? ? ? ? ? 89 47 70")) + 2);

			// make sure we keep track of the original invalidation state
			const auto oldInvalidationState = EnableInvalidateBoneCache;

			// is player bot?
			auto IsPlayerBot = [&]() -> bool {
				player_info_t info;
				if (Interfaces::m_pEngine->GetPlayerInfo(player->EntIndex(), &info))
					return info.fakeplayer;

				return false;
			};

			// show teammate lby
			if (player->IsTeammate(player) && !IsPlayerBot())
				player->m_angEyeAngles().y = player->m_flLowerBodyYawTarget();

			// attempt to resolve the player	
			if (!player->IsTeammate(C_CSPlayer::GetLocalPlayer()) && !IsPlayerBot()) {

				g_Resolver.ResolveYaw(player, current.Xor());

				g_Resolver.MatchShot(player, current.Xor(), previous.Xor());

				// predict lby updates
				g_Resolver.PredictBodyUpdates(player, current.Xor(), previous.Xor());

				bool bValid = previous.Xor();

				// we're sure that we resolved the player.
				if (bValid) {
					bool running = current.Xor()->m_iResolverMode == EResolverModes::RESOLVE_WALK && current.Xor()->m_vecVelocity.Length() > 70.f;
					current.Xor()->m_bResolved = current.Xor()->m_iResolverMode == EResolverModes::RESOLVE_LBY_UPDATE || current.Xor()->m_iResolverMode == EResolverModes::RESOLVE_LAST_LBY || 
						current.Xor()->m_iResolverMode == EResolverModes::RESOLVE_LBY || running;
				}
				else
					current.Xor()->m_bResolved = current.Xor()->m_iResolverMode == EResolverModes::RESOLVE_LBY_UPDATE;
			}

			player->UpdateClientSideAnimationEx();

			// we don't want to enable cache invalidation by accident
			EnableInvalidateBoneCache = oldInvalidationState;

			Interfaces::m_pGlobalVars->curtime = curtime;
			Interfaces::m_pGlobalVars->frametime = frametime;
		};

		SimulationRestore SimulationRecordBackup;
		SimulationRecordBackup.Setup(player);

		auto animState = player->m_PlayerAnimState();

		if (previous.IsValid()) {
			if (previous->m_bIsInvalid && current->m_fFlags & FL_ONGROUND) {
				animState->m_bOnGround = true;
				animState->m_bHitground = false;
			}

			if (previous.IsValid()) {
				if (previous->m_bIsInvalid && current->m_fFlags & FL_ONGROUND) {
					animState->m_bOnGround = true;
					animState->m_bHitground = false;
				}

				animState->m_flFeetCycle = previous->m_flFeetCycle;
				animState->m_flFeetYawRate = previous->m_flFeetYawRate;
				*(float*)(uintptr_t(animState) + 0x180) = previous->m_serverAnimOverlays[12].m_flWeight;

				std::memcpy(player->m_AnimOverlay().Base(), previous->m_serverAnimOverlays, sizeof(previous->m_serverAnimOverlays));
			}
			else {
				animState->m_flFeetCycle = current->m_flFeetCycle;
				animState->m_flFeetYawRate = current->m_flFeetYawRate;
				*(float*)(uintptr_t(animState) + 0x180) = current->m_serverAnimOverlays[12].m_flWeight;
			}
		}

		if (current->m_iChokeTicks > 1) {
			for (auto it = this->m_vecSimulationData.begin(); it < this->m_vecSimulationData.end(); it++) {
				m_bForceVelocity = true;
				const auto& simData = *it;
				if (simData.bOnGround) {
					player->m_fFlags() |= FL_ONGROUND;
				}
				else {
					player->m_fFlags() &= ~FL_ONGROUND;
				}

				player->m_vecOrigin() = simData.m_vecOrigin;
				player->m_flDuckAmount() = simData.m_flDuckAmount;
				player->m_vecVelocity() = simData.m_vecVelocity;
				player->SetAbsVelocity(simData.m_vecVelocity);
				player->SetAbsOrigin(simData.m_vecOrigin);
				player->m_flLowerBodyYawTarget() = simData.m_flLowerBodyYawTarget;

				UpdateAnimations(player, player->m_flOldSimulationTime() + Interfaces::m_pGlobalVars->interval_per_tick);

				m_bForceVelocity = false;
			}
		}
		else {
			m_bForceVelocity = true;
			this->player->SetAbsVelocity(current->m_vecAnimationVelocity);
			this->player->SetAbsOrigin(current->m_vecOrigin);
			this->player->m_flLowerBodyYawTarget() = current->m_flLowerBodyYawTarget;

			UpdateAnimations(player, player->m_flOldSimulationTime() + Interfaces::m_pGlobalVars->interval_per_tick);

			m_bForceVelocity = false;
		}

		SimulationRecordBackup.Apply(player);
		player->InvalidatePhysicsRecursive(8);
	}
}