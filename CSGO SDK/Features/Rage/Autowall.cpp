#include "Autowall.h"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Classes/weapon.hpp"

// IsBreakableEntity
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/sp/src/game/shared/obstacle_pushaway.cpp
bool Autowall::IsBreakable(C_BaseEntity* pEntity) {
	if (!pEntity || pEntity->m_entIndex == 0 || !pEntity->GetCollideable())
		return false;

	static uintptr_t uTakeDamage = *(uintptr_t*)((uintptr_t)Engine::Displacement.Function.m_uIsBreakable + 38);
	const uintptr_t uTakeDamageBackup = *(uint8_t*)((uintptr_t)pEntity + uTakeDamage);

	const ClientClass* pClientClass = pEntity->GetClientClass();
	if (pClientClass) {
		const char* name = pClientClass->m_pNetworkName;

		// CBreakableSurface, CBaseDoor, ...
		if (name[1] != 'F'
			|| name[4] != 'c'
			|| name[5] != 'B'
			|| name[9] != 'h') {
			*(uint8_t*)((uintptr_t)pEntity + uTakeDamage) = 2; /*DAMAGE_YES*/
		}
	}

	using fnIsBreakable = bool(__thiscall*)(C_BaseEntity*);
	const bool bResult = ((fnIsBreakable)Engine::Displacement.Function.m_uIsBreakable)(pEntity);
	*(uint8_t*)((uintptr_t)pEntity + uTakeDamage) = uTakeDamageBackup;

	return bResult;
}

bool Autowall::IsArmored(C_CSPlayer* player, int nHitgroup) {
	const bool bHasHelmet = player->m_bHasHelmet();
	const bool bHasHeavyArmor = player->m_bHasHeavyArmor();
	const float flArmorValue = player->m_ArmorValue();

	if (flArmorValue > 0) {
		switch (nHitgroup) {
		case Hitgroup_Generic:
		case Hitgroup_Chest:
		case Hitgroup_Stomach:
		case Hitgroup_LeftArm:
		case Hitgroup_RightArm:
		case Hitgroup_LeftLeg:
		case Hitgroup_RightLeg:
		case Hitgroup_Gear:
			return true;
			break;
		case Hitgroup_Head:
			return bHasHelmet || bHasHeavyArmor;
			break;
		default:
			return bHasHeavyArmor;
			break;
		}
	}

	return false;
}

// references CCSPlayer::TraceAttack and CCSPlayer::OnTakeDamage
float Autowall::ScaleDamage(C_CSPlayer* player, float flDamage, float flArmorRatio, int nHitgroup) {
	if (!player)
		return -1.f;

	C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer();

	if (!pLocal)
		return -1.f;

	C_WeaponCSBaseGun* pWeapon = (C_WeaponCSBaseGun*)pLocal->m_hActiveWeapon().Get();

	if (!pWeapon)
		return -1.f;

	const int nTeamNum = player->m_iTeamNum();
	float flHeadDamageScale = nTeamNum == TEAM_CT ? g_Vars.mp_damage_scale_ct_head->GetFloat() : g_Vars.mp_damage_scale_t_head->GetFloat();
	const float flBodyDamageScale = nTeamNum == TEAM_CT ? g_Vars.mp_damage_scale_ct_body->GetFloat() : g_Vars.mp_damage_scale_t_body->GetFloat();

	const bool bIsArmored = IsArmored(player, nHitgroup);
	const bool bHasHeavyArmor = player->m_bHasHeavyArmor();
	const bool bIsZeus = pWeapon->m_iItemDefinitionIndex() == WEAPON_ZEUS;

	const float flArmorValue = static_cast<float>(player->m_ArmorValue());

	if (!bIsZeus) {
		switch (nHitgroup) {
		case Hitgroup_Head:
			flDamage *= 4.0f * flHeadDamageScale;
			break;
		case Hitgroup_Stomach:
			flDamage *= 1.25f * flBodyDamageScale;
			break;
		case Hitgroup_Chest:
		case Hitgroup_LeftArm:
		case Hitgroup_RightArm:
		case Hitgroup_Gear:
			flDamage *= flBodyDamageScale;
			break;
		case Hitgroup_LeftLeg:
		case Hitgroup_RightLeg:
			flDamage = (flDamage * 0.75f) * flBodyDamageScale;
			break;
		default:
			break;
		}
	}

	if (bIsArmored) {
		float flArmorScale = 1.f;
		float flArmorBonusRatio = 0.5f;
		float flArmorRatioCalculated = flArmorRatio * 0.5f;
		float fDamageToHealth = 0.f;

		if (bHasHeavyArmor) {
			flArmorRatioCalculated = flArmorRatio * 0.25f;
			flArmorBonusRatio = 0.33f;
			flArmorScale = 0.33f;

			fDamageToHealth = (flDamage * flArmorRatioCalculated) * 0.85f;
		}
		else {
			fDamageToHealth = flDamage * flArmorRatioCalculated;
		}

		float fDamageToArmor = (flDamage - fDamageToHealth) * (flArmorScale * flArmorBonusRatio);

		// Does this use more armor than we have?
		if (fDamageToArmor > flArmorValue)
			fDamageToHealth = flDamage - (flArmorValue / flArmorBonusRatio);

		flDamage = fDamageToHealth;
	}

	return std::floor(flDamage);
}

void Autowall::TraceLine(const Vector& start, const Vector& end, uint32_t mask, ITraceFilter* ignore, CGameTrace* ptr) {
	Ray_t ray;
	ray.Init(start, end);
	Interfaces::m_pEngineTrace->TraceRay(ray, mask, ignore, ptr);
}

void util_clip_trace_to_players(const Vector& vec_abs_start, const Vector& vec_abs_end, unsigned int mask, ITraceFilter* filter, CGameTrace* tr)
{
	uint32_t clip_trace_to_players = 0;

	static auto dw_address = clip_trace_to_players;

	_asm
	{
		mov		eax, filter
		lea		ecx, tr
		push	ecx
		push	eax
		push	mask
		lea		edx, vec_abs_end
		lea		ecx, vec_abs_start
		call	dw_address
		add		esp, 0xC
	}
}

__forceinline float DistanceToRay(const Vector& vecPosition, const Vector& vecRayStart, const Vector& vecRayEnd, float* flAlong = NULL, Vector* vecPointOnRay = NULL) {
	Vector vecTo = vecPosition - vecRayStart;
	Vector vecDir = vecRayEnd - vecRayStart;
	float flLength = vecDir.Normalize();

	float flRangeAlong = DotProduct(vecDir, vecTo);
	if (flAlong) {
		*flAlong = flRangeAlong;
	}

	float flRange;

	if (flRangeAlong < 0.0f) {
		// off start point
		flRange = -vecTo.Length();

		if (vecPointOnRay) {
			*vecPointOnRay = vecRayStart;
		}
	}
	else if (flRangeAlong > flLength) {
		// off end point
		flRange = -(vecPosition - vecRayEnd).Length();

		if (vecPointOnRay) {
			*vecPointOnRay = vecRayEnd;
		}
	}
	else { // within ray bounds
		Vector vecOnRay = vecRayStart + vecDir * flRangeAlong;
		flRange = (vecPosition - vecOnRay).Length();

		if (vecPointOnRay) {
			*vecPointOnRay = vecOnRay;
		}
	}

	return flRange;
}


void Autowall::ClipTraceToPlayer(const Vector vecAbsStart, const Vector& vecAbsEnd, uint32_t iMask, ITraceFilter* pFilter, CGameTrace* pGameTrace, Encrypted_t<Autowall::C_FireBulletData> pData) {
	constexpr float flMaxRange = 60.0f, flMinRange = 0.0f;

	ICollideable* pCollideble = pData->m_TargetPlayer->GetCollideable();

	if (!pCollideble)
		return;

	// get bounding box
	const Vector vecObbMins = pCollideble->OBBMins();
	const Vector vecObbMaxs = pCollideble->OBBMaxs();
	const Vector vecObbCenter = (vecObbMaxs + vecObbMins) / 2.f;

	// calculate world space center
	const Vector vecPosition = vecObbCenter + pData->m_TargetPlayer->GetAbsOrigin();

	Ray_t Ray;
	Ray.Init(vecAbsStart, vecAbsEnd);

	// calculate distance to ray
	const float flRange = DistanceToRay(vecPosition, vecAbsStart, vecAbsEnd);

	if (flRange < 0.0f || flRange > 60.0f)
		return;

	CGameTrace playerTrace;
	Interfaces::m_pEngineTrace->ClipRayToEntity(Ray, iMask, pData->m_TargetPlayer, &playerTrace);
	if (pData->m_EnterTrace.fraction > playerTrace.fraction)
		pData->m_EnterTrace = playerTrace;
}

void Autowall::ClipTraceToPlayers(const Vector& vecAbsStart, const Vector& vecAbsEnd, uint32_t iMask, ITraceFilter* pFilter, CGameTrace* pGameTrace, float flMaxRange, float flMinRange) {
	float flSmallestFraction = pGameTrace->fraction;

	Vector vecDelta(vecAbsEnd - vecAbsStart);
	const float flDelta = vecDelta.Normalize();

	Ray_t Ray;
	Ray.Init(vecAbsStart, vecAbsEnd);

	for (int i = 0; i <= Interfaces::m_pGlobalVars->maxClients; ++i) {
		C_CSPlayer* pPlayer = C_CSPlayer::GetPlayerByIndex(i);
		if (!pPlayer || pPlayer->IsDormant() || pPlayer->IsDead())
			continue;

		if (pFilter && !pFilter->ShouldHitEntity(pPlayer, iMask))
			continue;

		ICollideable* pCollideble = pPlayer->GetCollideable();
		if (!pCollideble)
			continue;

		// get bounding box
		const Vector vecObbMins = pCollideble->OBBMins();
		const Vector vecObbMaxs = pCollideble->OBBMaxs();
		const Vector vecObbCenter = (vecObbMaxs + vecObbMins) / 2.f;

		// calculate world space center
		const Vector vecPosition = vecObbCenter + pPlayer->GetAbsOrigin();

		// calculate distance to ray
		const float flRange = DistanceToRay(vecPosition, vecAbsStart, vecAbsEnd);

		//if( flRange <= flMinRange || flRange >= flMaxRange )
		//	return;

		if (flRange < flMinRange || flRange > flMaxRange)
			return;

		CGameTrace playerTrace;
		Interfaces::m_pEngineTrace->ClipRayToEntity(Ray, iMask, pPlayer, &playerTrace);
		if (playerTrace.fraction < flSmallestFraction) {
			// we shortened the ray - save off the trace
			*pGameTrace = playerTrace;
			flSmallestFraction = playerTrace.fraction;
		}
	}
}

bool Autowall::TraceToExit(CGameTrace* pEnterTrace, Vector vecStartPos, Vector vecDirection, CGameTrace* pExitTrace) {
	constexpr float flMaxDistance = 90.f, flStepSize = 1.f;
	float flCurrentDistance = 0.f;

	int iFirstContents = 0;

	bool bIsWindow = 0;
	auto v23 = 0;

	do {
		// Add extra distance to our ray
		flCurrentDistance += flStepSize;

		// Multiply the direction vector to the distance so we go outwards, add our position to it.
		Vector vecEnd = vecStartPos + (vecDirection * flCurrentDistance);

		if (!iFirstContents)
			iFirstContents = Interfaces::m_pEngineTrace->GetPointContents(vecEnd, MASK_SHOT_PLAYER);

		int iPointContents = Interfaces::m_pEngineTrace->GetPointContents(vecEnd, MASK_SHOT_PLAYER);

		if (!(iPointContents & MASK_SHOT_HULL) || ((iPointContents & CONTENTS_HITBOX) && iPointContents != iFirstContents)) {

			// Let's setup our end position by deducting the direction by the extra added distance
			Vector vecStart = vecEnd - (vecDirection * flStepSize);

			// this gets a bit more complicated and expensive when we have to deal with displacements
			TraceLine(vecEnd, vecStart, MASK_SHOT_PLAYER, nullptr, pExitTrace);

			// note - dex; this is some new stuff added sometime around late 2017 ( 10.31.2017 update? ).
			//if ( g_Vars.sv_clip_penetration_traces_to_players->GetInt( ) )
			//	ClipTraceToPlayers( vecEnd, vecStart, MASK_SHOT_PLAYER, nullptr, pExitTrace, -60.f, -1.f );

			// we hit an ent's hitbox, do another trace.
			if (pExitTrace->startsolid && pExitTrace->surface.flags & SURF_HITBOX) {
				uint32_t filter_[4] = { *reinterpret_cast<uint32_t*> (Engine::Displacement.Function.m_TraceFilterSimple), uint32_t(C_CSPlayer::GetLocalPlayer()), 0, 0 };
				filter_[1] = reinterpret_cast<uint32_t>(pExitTrace->hit_entity);

				// do another trace, but skip the player to get the actual exit surface 
				TraceLine(vecStartPos, vecStart, MASK_SHOT_HULL, reinterpret_cast<CTraceFilter*>(filter_), pExitTrace);

				if (pExitTrace->DidHit() && !pExitTrace->startsolid) {
					vecEnd = pExitTrace->endpos;
					return true;
				}

				continue;
			}

			//Can we hit? Is the wall solid?
			if (pExitTrace->DidHit() && !pExitTrace->startsolid) {
				if (IsBreakable((C_BaseEntity*)pEnterTrace->hit_entity) && IsBreakable((C_BaseEntity*)pExitTrace->hit_entity))
					return true;

				if (pEnterTrace->surface.flags & SURF_NODRAW ||
					(!(pExitTrace->surface.flags & SURF_NODRAW) && pExitTrace->plane.normal.Dot(vecDirection) <= 1.f)) {
					const float flMultAmount = pExitTrace->fraction * 4.f;

					// get the real end pos
					vecStart -= vecDirection * flMultAmount;
					return true;
				}

				continue;
			}

			if (!pExitTrace->DidHit() || pExitTrace->startsolid) {
				if (pEnterTrace->DidHitNonWorldEntity() && IsBreakable((C_BaseEntity*)pEnterTrace->hit_entity)) {
					// if we hit a breakable, make the assumption that we broke it if we can't find an exit (hopefully..)
					// fake the end pos
					pExitTrace = pEnterTrace;
					pExitTrace->endpos = vecStartPos + vecDirection;
					return true;
				}
			}
		}
		// max pen distance is 90 units.
	} while (flCurrentDistance <= flMaxDistance);

	return false;
}

bool Autowall::HandleBulletPenetration(Encrypted_t<C_FireBulletData> data) {
	int iEnterMaterial = data->m_EnterSurfaceData->game.material;
	const int nPenetrationSystem = g_Vars.sv_penetration_type->GetInt();

	bool bContentsGrate = (data->m_EnterTrace.contents & CONTENTS_GRATE);
	bool bNoDrawSurf = (data->m_EnterTrace.surface.flags & SURF_NODRAW); // this is valve code :D!

	// check if bullet can penetrarte another entity
	if (data->m_iPenetrationCount == 0 &&
		!bContentsGrate &&
		!bNoDrawSurf &&
		iEnterMaterial != CHAR_TEX_GRATE &&
		iEnterMaterial != CHAR_TEX_GLASS)
		return true; // no, stop

	// if we hit a grate with iPenetration == 0, stop on the next thing we hit
	if (data->m_WeaponData->m_flPenetration <= 0.f || data->m_iPenetrationCount == 0)
		return true;

	// find exact penetration exit
	CGameTrace ExitTrace = { };
	if (!TraceToExit(&data->m_EnterTrace, data->m_EnterTrace.endpos, data->m_vecDirection, &ExitTrace)) {
		// ended in solid
		if ((Interfaces::m_pEngineTrace->GetPointContents(data->m_EnterTrace.endpos, MASK_SHOT_HULL) & MASK_SHOT_HULL) == 0)
			return true;
	}

	const surfacedata_t* pExitSurfaceData = Interfaces::m_pPhysSurface->GetSurfaceData(ExitTrace.surface.surfaceProps);

	if (!pExitSurfaceData)
		return true;

	const float flEnterPenetrationModifier = data->m_EnterSurfaceData->game.flPenetrationModifier;
	const float flExitPenetrationModifier = pExitSurfaceData->game.flPenetrationModifier;
	const float flExitDamageModifier = pExitSurfaceData->game.flDamageModifier;

	const int iExitMaterial = pExitSurfaceData->game.material;

	float flDamageModifier = 0.f;
	float flPenetrationModifier = 0.f;

	/*
	// percent of total damage lost automatically on impacting a surface
	flDamageModifier = 0.16f;
	flPenetrationModifier = ( flEnterPenetrationModifier + flExitPenetrationModifier ) * 0.5f;*/

	// new penetration method
	if (nPenetrationSystem == 1) {
		// percent of total damage lost automatically on impacting a surface

		/*
		flDamageModifier = 0.16f;
		flPenetrationModifier = ( flEnterPenetrationModifier + flExitPenetrationModifier ) * 0.5f;*/

		/*
		if( bContentsGrate || bNoDrawSurf || iEnterMaterial == CHAR_TEX_GLASS || iEnterMaterial == CHAR_TEX_GRATE ) {
			
			if( iEnterMaterial == CHAR_TEX_GLASS || iEnterMaterial == CHAR_TEX_GRATE ) {
				flPenetrationModifier = 3.0f;
				flDamageModifier = 0.05f;
			}
			else {
				flPenetrationModifier = 1.0f;
				flDamageModifier = 0.16f;
			}
		}*/

		if (iEnterMaterial == CHAR_TEX_GRATE || iEnterMaterial == CHAR_TEX_GLASS) {
			flPenetrationModifier = 3.0f;
			flDamageModifier = 0.05f;
		}

		else if (bContentsGrate || bNoDrawSurf) {
			flPenetrationModifier = 1.0f;
			flDamageModifier = 0.16f;
		}

		else {
			flPenetrationModifier = (flEnterPenetrationModifier + flExitPenetrationModifier) * 0.5f;
			flDamageModifier = 0.16f;
		}


		/*
		// for some weird reason some community servers have ff_damage_reduction_bullets > 0 but ff_damage_bullet_penetration == 0
		// so yeah, no shooting through teammates :)
		else if( iEnterMaterial == CHAR_TEX_FLESH && ( data->m_Player->IsTeammate( ( C_CSPlayer* )( data->m_EnterTrace.hit_entity ) ) ) &&
			g_Vars.ff_damage_reduction_bullets->GetFloat( ) >= 0.f ) {
			//Look's like you aren't shooting through your teammate today
			if( g_Vars.ff_damage_bullet_penetration->GetFloat( ) == 0.f )
				return true;

			//Let's shoot through teammates and get kicked for teamdmg! Whatever, atleast we did damage to the enemy. I call that a win.
			flPenetrationModifier = g_Vars.ff_damage_bullet_penetration->GetFloat( );
			flDamageModifier = 0.16f;
		}*/

		// if enter & exit point is wood we assume this is 
		// a hollow crate and give a penetration bonus
		if (iEnterMaterial == iExitMaterial) {
			if (iExitMaterial == CHAR_TEX_WOOD || iExitMaterial == CHAR_TEX_CARDBOARD)
				flPenetrationModifier = 3.0f;
			else if (iExitMaterial == CHAR_TEX_PLASTIC)
				flPenetrationModifier = 2.0f;
		}

		// calculate damage  
		const float flTraceDistance = (ExitTrace.endpos - data->m_EnterTrace.endpos).Length();
		const float flPenetrationMod = fmaxf(1.0 / flPenetrationModifier, 0.0f);
		const float flTotalLostDamage = (fmaxf(3.f / data->m_WeaponData->m_flPenetration, 0.f) *
			(flPenetrationMod * 3.f) + (data->m_flCurrentDamage * flDamageModifier)) +
			(((flTraceDistance * flTraceDistance) * flPenetrationMod) / 24);

		const float flClampedLostDamage = fmaxf(flTotalLostDamage, 0.f);

		if (flClampedLostDamage > data->m_flCurrentDamage)
			return true;

		// reduce damage power each time we hit something other than a grate
		if (flClampedLostDamage > 0.0f)
			data->m_flCurrentDamage -= flClampedLostDamage;

		// do we still have enough damage to deal?
		if (data->m_flCurrentDamage < 5.0f)
			return true;

		// penetration was successful
		// setup new start end parameters for successive trace
		data->m_vecStart = ExitTrace.endpos;
		--data->m_iPenetrationCount;
		return false;
	}
	else {
		// since some railings in de_inferno are CONTENTS_GRATE but CHAR_TEX_CONCRETE, we'll trust the
		// CONTENTS_GRATE and use a high damage modifier.
		if (bContentsGrate || bNoDrawSurf) {
			// If we're a concrete grate (TOOLS/TOOLSINVISIBLE texture) allow more penetrating power.
			flPenetrationModifier = 1.0f;
			flDamageModifier = 0.99f;
		}
		else {
			if (flExitPenetrationModifier < flPenetrationModifier) {
				flPenetrationModifier = flExitPenetrationModifier;
			}
			if (flExitDamageModifier < flDamageModifier) {
				flDamageModifier = flExitDamageModifier;
			}
		}

		// if enter & exit point is wood or metal we assume this is 
		// a hollow crate or barrel and give a penetration bonus
		if (iEnterMaterial == iExitMaterial) {
			if (iExitMaterial == CHAR_TEX_WOOD ||
				iExitMaterial == CHAR_TEX_METAL) {
				flPenetrationModifier *= 2;
			}
		}

		float flTraceDistance = (ExitTrace.endpos - data->m_EnterTrace.endpos).Length();

		// check if bullet has enough power to penetrate this distance for this material
		if (flTraceDistance > (data->m_WeaponData->m_flPenetration * flPenetrationModifier))
			return true; // bullet hasn't enough power to penetrate this distance

		// reduce damage power each time we hit something other than a grate
		data->m_flCurrentDamage *= flDamageModifier;


		// do we still have enough damage to deal?
		if (data->m_flCurrentDamage < 5.0f)
			return true;

		// penetration was successful
		// setup new start end parameters for successive trace
		data->m_vecStart = ExitTrace.endpos;
		--data->m_iPenetrationCount;
		return false;
	}
}


__forceinline bool IsValidHitgroup(int index) {
	if ((index >= Hitgroup_Head && index <= Hitgroup_RightLeg) || index == Hitgroup_Gear)
		return true;

	return false;
}

float Autowall::FireBullets(Encrypted_t<C_FireBulletData> data) {
	constexpr float rayExtension = 40.f;

	//This gets set in FX_Firebullets to 4 as a pass-through value.
	//CS:GO has a maximum of 4 surfaces a bullet can pass-through before it 100% stops.
	//Excerpt from Valve: https://steamcommunity.com/sharedfiles/filedetails/?id=275573090
	//"The total number of surfaces any bullet can penetrate in a single flight is capped at 4." -CS:GO Official

	if (!data->m_Weapon) {
		data->m_Weapon = (C_WeaponCSBaseGun*)(data->m_Player->m_hActiveWeapon().Get());
		if (data->m_Weapon) {
			data->m_WeaponData = data->m_Weapon->GetCSWeaponData().Xor();
		}
	}

	data->m_flTraceLength = 0.f;
	data->m_flCurrentDamage = static_cast<float>(data->m_WeaponData->m_iWeaponDamage);

	CTraceFilter TraceFilter;
	TraceFilter.pSkip = data->m_Player;

	if (!data->m_Filter)
		data->m_Filter = &TraceFilter;

	data->m_flMaxLength = data->m_WeaponData->m_flWeaponRange;

	g_Vars.globals.m_InHBP = true;

	while (data->m_flCurrentDamage >= 1.f) {
		// calculate max bullet range
		data->m_flMaxLength -= data->m_flTraceLength;

		// create end point of bullet
		Vector vecEnd = data->m_vecStart + data->m_vecDirection * data->m_flMaxLength;

		TraceLine(data->m_vecStart, vecEnd, MASK_SHOT_PLAYER, &TraceFilter, &data->m_EnterTrace);

		// create extended end point
		Vector vecEndExtended = vecEnd + data->m_vecDirection * rayExtension;

		// NOTICE: can remove valve`s hack aka bounding box fix
		// Check for player hitboxes extending outside their collision bounds
		if (data->m_TargetPlayer) {
			// clip trace to one player
			ClipTraceToPlayer(data->m_vecStart, vecEndExtended, MASK_SHOT_PLAYER, data->m_Filter, &data->m_EnterTrace, data);
		}
		else {
			ClipTraceToPlayers(data->m_vecStart, vecEndExtended, MASK_SHOT_PLAYER, data->m_Filter, &data->m_EnterTrace);
		}

		if (data->m_EnterTrace.fraction == 1.f)
			break;  // we didn't hit anything, stop tracing and shoot

		// calculate the damage based on the distance the bullet traveled.
		data->m_flTraceLength += data->m_EnterTrace.fraction * data->m_flMaxLength;

		// let's make our damage drop off the further away the bullet is.
		if (!data->m_bShouldIgnoreDistance)
			data->m_flCurrentDamage *= powf(data->m_WeaponData->m_flRangeModifier, (data->m_flTraceLength * 0.002f));

		C_CSPlayer* pHittedPlayer = ToCSPlayer((C_BasePlayer*)data->m_EnterTrace.hit_entity);

		const int nHitGroup = data->m_EnterTrace.hitgroup;
		const bool bHitgroupIsValid = data->m_Weapon->m_iItemDefinitionIndex() == WEAPON_ZEUS ? (nHitGroup >= Hitgroup_Generic && nHitGroup < Hitgroup_Neck) : IsValidHitgroup(nHitGroup);
		const bool bTargetIsValid = !data->m_TargetPlayer || (pHittedPlayer != nullptr && pHittedPlayer->m_entIndex == data->m_TargetPlayer->m_entIndex);

		if (pHittedPlayer != nullptr) {
			if (bTargetIsValid && bHitgroupIsValid && pHittedPlayer->IsPlayer() && pHittedPlayer->m_entIndex <= Interfaces::m_pGlobalVars->maxClients && pHittedPlayer->m_entIndex > 0) {
				data->m_flCurrentDamage = ScaleDamage(pHittedPlayer, data->m_flCurrentDamage, data->m_WeaponData->m_flArmorRatio, data->m_Weapon->m_iItemDefinitionIndex() == WEAPON_ZEUS ? Hitgroup_Generic : nHitGroup);
				data->m_iHitgroup = nHitGroup;


				g_Vars.globals.m_InHBP = false;
				return data->m_flCurrentDamage;
			}
		}

		bool bCanPenetrate = data->m_bPenetration;
		if (!data->m_bPenetration)
			bCanPenetrate = data->m_EnterTrace.contents & CONTENTS_WINDOW;

		if (!bCanPenetrate)
			break;

		data->m_EnterSurfaceData = Interfaces::m_pPhysSurface->GetSurfaceData(data->m_EnterTrace.surface.surfaceProps);

		if (!data->m_EnterSurfaceData)
			break;

		// check if we reach penetration distance, no more penetrations after that
		// or if our modifier is super low, just stop the bullet
		if ((data->m_flTraceLength > 3000.f && data->m_WeaponData->m_flPenetration > 0.f) || 
			data->m_EnterSurfaceData->game.flPenetrationModifier < 0.1f) {
			data->m_iPenetrationCount = 0;
			break;
		}

		bool bIsBulletStopped = HandleBulletPenetration(data);

		if (bIsBulletStopped)
			break;

	}

	g_Vars.globals.m_InHBP = false;
	return -1.f;
}