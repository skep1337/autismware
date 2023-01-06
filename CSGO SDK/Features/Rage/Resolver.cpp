#include "Resolver.hpp"
#include "../../SDK/CVariables.hpp"
#include "../Visuals/CChams.hpp"
#include "../Rage/AntiAim.hpp"
#include "../Rage/Ragebot.hpp"
#include "../Rage/Autowall.h"
#include "../Visuals/EventLogger.hpp"
#include "../../SDK/RayTracer.h"

namespace Engine {
	CResolver g_Resolver;
	CResolverData g_ResolverData[65];

	// resolver ported from supremacy autismware (p100).

	int last_ticks[65];
	int CResolver::GetChokedPackets(C_CSPlayer* player)
	{
		// make sure they have no fake for at least 2 ticks in a row.
		float lastsim = player->m_flSimulationTime() - Interfaces::m_pGlobalVars->interval_per_tick;
		float lastoldsim = player->m_flOldSimulationTime() - Interfaces::m_pGlobalVars->interval_per_tick;

		auto oldticks = TIME_TO_TICKS(lastsim - lastoldsim);

		auto ticks = TIME_TO_TICKS(player->m_flSimulationTime() - player->m_flOldSimulationTime());
		if (ticks == 0 && oldticks == 0 && last_ticks[player->EntIndex()] > 0) {
			return last_ticks[player->EntIndex()] - 1;
		}
		else {
			last_ticks[player->EntIndex()] = ticks;
			return ticks;
		}
	}

	void CResolver::ResolveYaw(C_CSPlayer* player, C_AnimationRecord* record)
	{
		bool fake = GetChokedPackets(player) > 1;
		float speed = record->m_vecAnimationVelocity.Length();

		if (fake) {
			if ((record->m_fFlags & FL_ONGROUND) && speed > 0.1f && !record->m_bFakeWalking && !record->m_bFakeFlicking)
				ResolveWalk(player, record);

			else if ((record->m_fFlags & FL_ONGROUND) && (speed <= 0.1f || record->m_bFakeWalking || record->m_bFakeFlicking))
				ResolveStand(player, record);

			else
				ResolveAir(player, record);
		}
		else // no fake yaw detected.
		{
			record->m_iResolverMode = EResolverModes::RESOLVE_NONE;
			record->m_resolver_mode = XorStr("");
		}

		// write potentially resolved angles.
		player->m_angEyeAngles().y = Math::AngleNormalize(record->m_angEyeAngles.y);
	}

	void CResolver::MatchShot(C_CSPlayer* player, C_AnimationRecord* record, C_AnimationRecord* prev)
	{
		auto pLocal = C_CSPlayer::GetLocalPlayer();
		if (!pLocal)
			return;

		auto anim_data = AnimationSystem::Get()->GetAnimationData(player->m_entIndex);

		if (anim_data->m_AnimationRecord.size() < 2)
			return;

		auto weapon = (C_WeaponCSBaseGun*)(player->m_hActiveWeapon().Get());
		if (!weapon)
			return;

		if (!record->m_bIsShooting && !prev->m_bIsShooting)
			record->m_angPrevEyeAngles.x = record->m_angEyeAngles.x;
		else
			record->m_angEyeAngles.x = record->m_angPrevEyeAngles.x;
	}

	bool CResolver::ShouldUseFreestand(C_CSPlayer* player, C_AnimationRecord* record) // allows freestanding if not in open
	{
		auto pLocal = C_CSPlayer::GetLocalPlayer();
		if (!pLocal)
			return false;

		if (!player || player->IsDead())
			return false;

		if (player->IsDormant())
			return false;

		// don't resolve if no fake
		bool fake = GetChokedPackets(player) > 1;
		if (!fake)
			return false;

		// externs
		Vector src3D, dst3D, forward, right, up, src, dst;
		float back_two, right_two, left_two;
		CGameTrace tr;
		CTraceFilterSimple filter;

		// get predicted away angle for the player.
		Vector angAway;
		auto m_vecOrigin = player->m_vecOrigin();
		Math::VectorAngles(pLocal->m_vecOrigin() - m_vecOrigin, angAway);

		// angle vectors
		Math::AngleVectors(QAngle(0, angAway.y, 0), forward, right, up);

		// filtering
		filter.SetPassEntity(player);
		src3D = player->GetEyePosition();
		dst3D = src3D + (forward * 100);

		// back engine tracers
		Interfaces::m_pEngineTrace->TraceRay(Ray_t(src3D, dst3D), MASK_SHOT_BRUSHONLY | MASK_OPAQUE_AND_NPCS, &filter, &tr);
		back_two = (tr.endpos - tr.startpos).Length();

		// right engine tracers
		Interfaces::m_pEngineTrace->TraceRay(Ray_t(src3D + right * 35, dst3D + right * 35), MASK_SHOT_BRUSHONLY | MASK_OPAQUE_AND_NPCS, &filter, &tr);
		right_two = (tr.endpos - tr.startpos).Length();

		// left engine tracers
		Interfaces::m_pEngineTrace->TraceRay(Ray_t(src3D - right * 35, dst3D - right * 35), MASK_SHOT_BRUSHONLY | MASK_OPAQUE_AND_NPCS, &filter, &tr);
		left_two = (tr.endpos - tr.startpos).Length();

		// pick side
		if (left_two > right_two) {
			this->bFacingleft = true;
			this->bFacingright = false;
			return true;
		}
		else if (right_two > left_two) {
			this->bFacingright = true;
			this->bFacingleft = false;
			return true;
		}
		else
			return false;
	}

	void CResolver::Freestand(C_CSPlayer* player, C_AnimationRecord* record)
	{
		auto pLocal = C_CSPlayer::GetLocalPlayer();
		if (!pLocal)
			return;

		// don't resolve if no fake
		bool fake = GetChokedPackets(player) > 1;
		if (!fake)
			return;

		// constants
		constexpr float STEP{ 4.f };
		constexpr float RANGE{ 32.f };

		auto lag_data = Engine::LagCompensation::Get()->GetLagData(player->m_entIndex);
		if (!lag_data.IsValid())
			return;

		// best target.
		Vector enemypos = player->GetEyePosition();
		// get best origin based on target.
		auto m_vecOrigin = player->m_vecOrigin();

		// get predicted away angle for the player.
		Vector angAway;
		Math::VectorAngles(pLocal->m_vecOrigin() - m_vecOrigin, angAway);

		// construct vector of angles to test.
		std::vector< AdaptiveAngle > angles{ };
		angles.emplace_back(angAway.y + 180.f);
		angles.emplace_back(angAway.y + 90.f);
		angles.emplace_back(angAway.y - 90.f);

		// start the trace at the your shoot pos.
		Vector start = g_Vars.globals.m_vecFixedEyePosition;

		// see if we got any valid result.
		// if this is false the path was not obstructed with anything.
		bool valid{ false };
		// iterate vector of angles.

		for (auto it = angles.begin(); it != angles.end(); ++it) {
			// compute the 'rough' estimation of where our head will be.
			Vector end{ enemypos.x + std::cos(DEG2RAD(it->m_yaw)) * RANGE,
				enemypos.y + std::sin(DEG2RAD(it->m_yaw)) * RANGE,
				enemypos.z };

			// draw a line for debugging purposes.
			//g_csgo.m_debug_overlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.1f );

			// compute the direction.
			Vector dir = end - start;
			float len = dir.Normalize();

			// should never happen.

			if (len <= 0.f)
				continue;

			// step thru the total distance, 4 units per step.
			for (float i{ 0.f }; i < len; i += STEP) {
				// get the current step position.
				Vector point = start + (dir * i);

				// get the contents at this point.
				int contents = Interfaces::m_pEngineTrace->GetPointContents(point, MASK_SHOT_HULL);

				// contains nothing that can stop a bullet.
				if (!(contents & MASK_SHOT_HULL))
					continue;

				float mult = 1.f;

				// over 65% of the total length.
				if (i > (len * 0.65f))

					mult = 1.25f;

				// over 75% of the total length.
				if (i > (len * 0.75f))

					mult = 1.5f;

				// over 90% of the total length.
				if (i > (len * 0.9f))
					mult = 2.f;

				// append 'penetrated distance'.
				it->m_dist += (STEP * mult);

				// mark that we found anything.
				valid = true;

			}
		}

		if (!valid) {
			record->m_angEyeAngles.y = angAway.y + 190.f;
			return;

		}

		// put the most distance at the front of the container.

		std::sort(angles.begin(), angles.end(),

			[](const AdaptiveAngle& a, const AdaptiveAngle& b) {

				return a.m_dist > b.m_dist;

			});

		// the best angle should be at the front now.
		AdaptiveAngle* best = &angles.front();

		// set angles.
		if (lag_data->m_iMissedShotsFreestand < 1)
			record->m_angEyeAngles.y = best->m_yaw;
		else if (this->bFacingright && lag_data->m_iMissedShotsFreestand < 2)
			record->m_angEyeAngles.y = best->m_yaw + 90.f;
		else if (this->bFacingleft && lag_data->m_iMissedShotsFreestand < 2)
			record->m_angEyeAngles.y = best->m_yaw - 90.f;
		else if (lag_data->m_iMissedShotsFreestand == 2 || lag_data->m_iMissedShotsFreestand == 4)
			record->m_angEyeAngles.y = best->m_yaw + 180.f;
		else
			record->m_angEyeAngles.y = best->m_yaw;
		return;
	}

	void CResolver::ResolveStand(C_CSPlayer* player, C_AnimationRecord* record)
	{
		auto local = C_CSPlayer::GetLocalPlayer();
		if (!local)
			return;

		// get resolver data.
		auto& lasttick = g_ResolverData[player->EntIndex() - 1];
		auto& data = g_ResolverData[player->EntIndex()];
		float speed = record->m_vecAnimationVelocity.Length();

		// get records.
		auto anim_data = AnimationSystem::Get()->GetAnimationData(player->m_entIndex);

		// get last move time.
		float delta = player->m_flAnimationTime() - g_ResolverData[player->EntIndex()].m_sMoveData.m_flAnimTime;

		// get predicted away angle for the player.
		Vector angAway;
		Math::VectorAngles(local->m_vecOrigin() - player->m_vecOrigin(), angAway);

		// get lag data.
		Encrypted_t<Engine::C_EntityLagData> pLagData = Engine::LagCompensation::Get()->GetLagData(player->EntIndex());
		if (!pLagData.IsValid()) {
			return;
		}

		// we have a valid moving record.
		if (g_ResolverData[player->EntIndex()].m_sMoveData.m_flSimulationTime > 0.f) {
			Vector delta = g_ResolverData[player->EntIndex()].m_sMoveData.m_vecOrigin - record->m_vecOrigin;

			// check if moving record is close.
			if (delta.Length() <= 128.f && !record->m_bFakeFlicking && !record->m_bFakeWalking) {
				// indicate that we are using the moving lby.
				data.m_bCollectedValidMoveData = true;
			}
		}

		// expire last move after 0.75 secs if not in open.
		if (data.m_bCollectedValidMoveData && pLagData->m_iMissedShotsLastmove < 1 && (delta < 0.75f || !(player->m_fFlags() & FL_ONGROUND) || !ShouldUseFreestand(player, record)))
		{
			record->m_iResolverMode = EResolverModes::RESOLVE_LAST_LBY;
			record->m_resolver_mode = XorStr("LAST MOVE");
			record->m_angEyeAngles.y = player->m_angEyeAngles().y = data.m_sMoveData.m_flLowerBodyYawTarget;
		}
		else
		{
			if (ShouldUseFreestand(player, record)) // if freestand would be useful.
			{
				record->m_iResolverMode = EResolverModes::RESOLVE_FREESTAND;
				record->m_resolver_mode = XorStr("FREESTAND");
				Freestand(player, record);
				return;
			}
			else
			{
				record->m_iResolverMode = EResolverModes::RESOLVE_BRUTEFORCE;
				record->m_resolver_mode = XorStr("BRUTE");
				switch (pLagData->m_iMissedShotsBrute % 3) {

				case 0:
					record->m_angEyeAngles.y = angAway.y + 180.f;
					break;

				case 1:
					record->m_angEyeAngles.y = angAway.y - 135.f;
					break;

				case 2:
					record->m_angEyeAngles.y = angAway.y + 135.f;
					break;

				default:
					break;
				}
			}
		}
	}

	void CResolver::PredictBodyUpdates(C_CSPlayer* player, C_AnimationRecord* record, C_AnimationRecord* prev)
	{
		auto local = C_CSPlayer::GetLocalPlayer();
		if (!local)
			return;

		// nah
		if (local->IsDead())
		{
			Engine::g_ResolverData[player->EntIndex()].m_bPredictingUpdates = false;
			Engine::g_ResolverData[player->EntIndex()].m_bCollectedValidMoveData = false;
			return;
		}

		// get lag data about this player
		Encrypted_t<Engine::C_EntityLagData> pLagData = Engine::LagCompensation::Get()->GetLagData(player->m_entIndex);
		if (!pLagData.IsValid()) {
			Engine::g_ResolverData[player->EntIndex()].m_bPredictingUpdates = false;
			return;
		}

		// get records.
		auto anim_data = AnimationSystem::Get()->GetAnimationData(player->m_entIndex);

		if (anim_data->m_AnimationRecord.size() < 2)
			return;

		if (!(player->m_fFlags() & FL_ONGROUND) || record->m_bFakeFlicking) {
			Engine::g_ResolverData[player->EntIndex()].m_bPredictingUpdates = false;
			return;
		}

		C_AnimationLayer* current_layer = &player->m_AnimOverlay()[3];
		C_AnimationLayer* previous_layer = &prev->m_serverAnimOverlays[3];

		// check if the player is walking
		if (record->m_vecVelocity.Length() >= 0.1f) {
			// predict the first flick they have to do after they stop moving
			Engine::g_ResolverData[player->EntIndex()].m_flNextBodyUpdate = player->m_flAnimationTime() + 0.22f;
			Engine::g_ResolverData[player->EntIndex()].m_bPredictingUpdates = false;
			return;
		}

		// not breaking lby
		if (current_layer->m_flCycle > 0.987f && previous_layer->m_flCycle > 0.987f && current_layer->m_flWeight < 0.01f && previous_layer->m_flWeight < 0.01f && !(player->GetSequenceActivity(current_layer->m_nSequence) == 979))
		{
			record->m_iResolverMode = EResolverModes::RESOLVE_LBY;
			record->m_resolver_mode = XorStr("LBY");
			Engine::g_ResolverData[player->EntIndex()].m_bPredictingUpdates = false;
			record->m_angEyeAngles.y = player->m_angEyeAngles().y = record->m_flLowerBodyYawTarget;
			return;
		}
		// cycle check to make sure we dont miss delayed (> 1.1) break timers.
		else if (current_layer->m_flCycle < 0.25f && record->m_flLowerBodyYawTarget != prev->m_flLowerBodyYawTarget) // literally never misses :O
		{
			record->m_iResolverMode = EResolverModes::RESOLVE_LBY_UPDATE;
			record->m_resolver_mode = XorStr("FLICK");
			Engine::g_ResolverData[player->EntIndex()].m_bPredictingUpdates = true;
			Engine::g_ResolverData[player->EntIndex()].m_flNextBodyUpdate = player->m_flAnimationTime() + 1.1f;
			record->m_angEyeAngles.y = record->m_angLastFlick.y = player->m_angEyeAngles().y = record->m_flLowerBodyYawTarget;
		}
	}

	void CResolver::ResolveWalk(C_CSPlayer* player, C_AnimationRecord* record)
	{
		auto local = C_CSPlayer::GetLocalPlayer();
		if (!local)
			return;

		// apply lby to eyeangles.
		record->m_iResolverMode = EResolverModes::RESOLVE_WALK;
		record->m_resolver_mode = XorStr("MOVING");
		record->m_angEyeAngles.y = player->m_angEyeAngles().y = record->m_flLowerBodyYawTarget;
		Engine::g_ResolverData[player->EntIndex()].m_bPredictingUpdates = false;
		Engine::g_ResolverData[player->EntIndex()].m_flNextBodyUpdate = player->m_flAnimationTime() + 0.22f;

		// get lag data.
		Encrypted_t<Engine::C_EntityLagData> pLagData = Engine::LagCompensation::Get()->GetLagData(player->EntIndex());
		if (pLagData.IsValid()) {
			// predict the next time if they stop moving.
			pLagData->m_iMissedShotsLBY = 0;
			pLagData->m_iMissedShotsLastmove = 0;
			pLagData->m_iMissedShotsFreestand = 0;
			pLagData->m_iMissedShotsBrute = 0;
			pLagData->m_iMissedShotsAir = 0;
			pLagData->m_iMissedShotsDistort = 0;
			pLagData->m_iMissedShots = 0;
		}

		// store the data about the moving player, we need to because it contains crucial info
		// that we will have to later on use in our resolver.
		g_ResolverData[player->EntIndex()].m_sMoveData.m_flAnimTime = player->m_flAnimationTime();
		g_ResolverData[player->EntIndex()].m_sMoveData.m_vecOrigin = record->m_vecOrigin;
		g_ResolverData[player->EntIndex()].m_sMoveData.m_flLowerBodyYawTarget = record->m_flLowerBodyYawTarget;
		g_ResolverData[player->EntIndex()].m_sMoveData.m_flSimulationTime = record->m_flSimulationTime;
		g_ResolverData[player->EntIndex()].m_bCollectedValidMoveData = true;
	}

	void CResolver::ResolveAir(C_CSPlayer* player, C_AnimationRecord* record)
	{
		auto local = C_CSPlayer::GetLocalPlayer();
		if (!local)
			return;

		auto& data = g_ResolverData[player->EntIndex()];

		Encrypted_t<Engine::C_EntityLagData> pLagData = Engine::LagCompensation::Get()->GetLagData(player->EntIndex());
		if (!pLagData.IsValid()) {
			return;
		}

		// get predicted away angle for the player.
		Vector angAway;
		Math::VectorAngles(local->m_vecOrigin() - player->m_vecOrigin(), angAway);

		// they have barely any speed. 
		if (record->m_vecAnimationVelocity.Length2D() < 30.f)
		{
			// invoke our stand resolver.
			ResolveStand(player, record);
			return;
		}

		record->m_iResolverMode = RESOLVE_AIR;
		record->m_resolver_mode = XorStr("AIR");

		if (pLagData->m_iMissedShotsAir < 1)
			record->m_angEyeAngles.y = player->m_angEyeAngles().y = record->m_flLowerBodyYawTarget;
		else
			record->m_angEyeAngles.y = player->m_angEyeAngles().y = angAway.y + 180.f;
	}
}