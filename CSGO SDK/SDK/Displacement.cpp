#include "Displacement.hpp"
#include "Classes/PropManager.hpp"
#include "../Utils/FnvHash.hpp"

DllInitializeData Engine::Displacement{ };

namespace Engine
{
	__forceinline uintptr_t CallableFromRelative( DWORD nAddress ) {
		return nAddress + *( DWORD* )( nAddress + 1 ) + 5;
	}

	void Create( ) {
		auto& pPropManager = PropManager::Instance( );

		auto image_vstdlib = GetModuleHandleA( XorStr( "vstdlib.dll" ) );
		auto image_client = ( std::uintptr_t )GetModuleHandleA( XorStr( "client.dll" ) );
		auto image_engine = ( std::uintptr_t )GetModuleHandleA( XorStr( "engine.dll" ) );
		auto image_server = ( std::uintptr_t )GetModuleHandleA( XorStr( "server.dll" ) );
		auto image_shaderapidx9 = ( std::uintptr_t )GetModuleHandleA( XorStr( "shaderapidx9.dll" ) );

		// TODO: datamap
		Displacement.C_BaseEntity.m_MoveType = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_nRenderMode" ) ) + 1;
		Displacement.C_BaseEntity.m_rgflCoordinateFrame = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_CollisionGroup" ) ) - 0x30;

		Displacement.DT_BaseEntity.m_iTeamNum = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_iTeamNum" ) );
		Displacement.DT_BaseEntity.m_vecOrigin = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_vecOrigin" ) );
		Displacement.DT_BaseEntity.m_flSimulationTime = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_flSimulationTime" ) );
		Displacement.DT_BaseEntity.m_fEffects = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_fEffects" ) );
		Displacement.DT_BaseEntity.m_iEFlags = Displacement.DT_BaseEntity.m_fEffects - 0x8;
		Displacement.DT_BaseEntity.m_hOwnerEntity = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_hOwnerEntity" ) );
		Displacement.DT_BaseEntity.moveparent = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "moveparent" ) );
		Displacement.DT_BaseEntity.m_nModelIndex = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_nModelIndex" ) );
		Displacement.DT_BaseEntity.m_Collision = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_Collision" ) );
		Displacement.DT_BaseEntity.m_CollisionGroup = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_CollisionGroup" ) );
		Displacement.DT_BaseEntity.m_flAnimTime = pPropManager->GetOffset( XorStr( "DT_BaseEntity" ), XorStr( "m_flAnimTime" ) );

		Displacement.DT_BaseWeaponWorldModel.m_hCombatWeaponParent = pPropManager->GetOffset( XorStr( "DT_BaseWeaponWorldModel" ), XorStr( "m_hCombatWeaponParent" ) );

		auto m_hLightingOrigin = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_hLightingOrigin" ) );
		auto m_nForceBone = pPropManager->GetOffset( XorStr( "DT_BaseAnimating" ), XorStr( "m_nForceBone" ) );
		Displacement.C_BaseAnimating.InvalidateBoneCache = Memory::Scan( image_client, XorStr( "80 3D ? ? ? ? ? 74 16 A1 ? ? ? ? 48 C7 81" ) );

		Displacement.C_BaseAnimating.m_BoneAccessor = m_nForceBone + 0x1C; // todo
		Displacement.C_BaseAnimating.m_iMostRecentModelBoneCounter = *( int* )( Displacement.C_BaseAnimating.InvalidateBoneCache + 0x1B );
		Displacement.C_BaseAnimating.m_iPrevBoneMask = m_nForceBone + 0x10;
		Displacement.C_BaseAnimating.m_iAccumulatedBoneMask = m_nForceBone + 0x14;
		Displacement.C_BaseAnimating.m_bIsJiggleBonesEnabled = m_hLightingOrigin - 0x18;
		Displacement.C_BaseAnimating.m_iOcclusionFramecount = 0xA30;
		Displacement.C_BaseAnimating.m_iOcclusionFlags = 0xA28;
		Displacement.C_BaseAnimating.m_flLastBoneSetupTime = *( int* )( Displacement.C_BaseAnimating.InvalidateBoneCache + 0x11 );
		Displacement.C_BaseAnimating.m_CachedBoneData = *( int* )( Memory::Scan( image_client, XorStr( "FF B7 ?? ?? ?? ?? 52" ) ) + 2 ) + 0x4;
		Displacement.C_BaseAnimating.m_AnimOverlay = *( int* )( Memory::Scan( image_client, XorStr( "8B 89 ?? ?? ?? ?? 8D 0C D1" ) ) + 2 );

		auto BoneSnapshotsCall = Memory::Scan( image_client, XorStr( "8D 8F ?? ?? ?? ?? 6A 01 C7 87" ) );
		Displacement.C_BaseAnimating.m_pFirstBoneSnapshot = *( int* )( BoneSnapshotsCall + 0x2 );
		Displacement.C_BaseAnimating.m_pSecondBoneSnapshot = *( int* )( BoneSnapshotsCall + 0x1B );

		auto CacheBoneData = Memory::Scan( image_client, XorStr( "8D 87 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 06" ) );
		Displacement.C_BaseAnimating.m_nCachedBonesPosition = *( int* )( CacheBoneData + 0x2 ) + 0x4;
		Displacement.C_BaseAnimating.m_nCachedBonesRotation = *( int* )( CacheBoneData + 0x25 ) + 0x4;
		Displacement.C_BaseAnimating.m_pStudioHdr = *( int* )( Memory::Scan( image_client, XorStr( "8B B7 ?? ?? ?? ?? 89 74 24 20" ) ) + 0x2 ) + 0x4;
		Displacement.C_BaseAnimating.m_bShouldDraw = *( int* )( Memory::Scan( image_client, XorStr( "FF 15 ?? ?? ?? ?? 80 BE ?? ?? ?? ?? ?? 0F 84 ?? ?? ?? ??" ) ) + 0x8 );
		Displacement.C_BaseAnimating.m_pBoneMerge = *( int* )( Memory::Scan( image_client, XorStr( "89 86 ?? ?? ?? ?? E8 ?? ?? ?? ?? FF 75 08" ) ) + 2 );
		Displacement.C_BaseAnimating.m_pIk = *( int* )( Memory::Scan( image_client, XorStr( "8B 8F ?? ?? ?? ?? 89 4C 24 1C" ) ) + 2 ) + 4;

		Displacement.DT_BaseAnimating.m_bClientSideAnimation = pPropManager->GetOffset( XorStr( "DT_BaseAnimating" ), XorStr( "m_bClientSideAnimation" ) );
		Displacement.DT_BaseAnimating.m_flPoseParameter = pPropManager->GetOffset( XorStr( "DT_BaseAnimating" ), XorStr( "m_flPoseParameter" ) );
		Displacement.DT_BaseAnimating.m_nHitboxSet = pPropManager->GetOffset( XorStr( "DT_BaseAnimating" ), XorStr( "m_nHitboxSet" ) );
		Displacement.DT_BaseAnimating.m_flCycle = pPropManager->GetOffset( XorStr( "DT_BaseAnimating" ), XorStr( "m_flCycle" ) );
		Displacement.DT_BaseAnimating.m_nSequence = pPropManager->GetOffset( XorStr( "DT_BaseAnimating" ), XorStr( "m_nSequence" ) );
		Displacement.DT_BaseAnimating.m_flEncodedController = pPropManager->GetOffset( XorStr( "DT_BaseAnimating" ), XorStr( "m_flEncodedController" ) );

		Displacement.DT_BaseCombatCharacter.m_hActiveWeapon = pPropManager->GetOffset( XorStr( "DT_BaseCombatCharacter" ), XorStr( "m_hActiveWeapon" ) );
		Displacement.DT_BaseCombatCharacter.m_flNextAttack = pPropManager->GetOffset( XorStr( "DT_BaseCombatCharacter" ), XorStr( "m_flNextAttack" ) );
		Displacement.DT_BaseCombatCharacter.m_hMyWeapons = pPropManager->GetOffset( XorStr( "DT_BaseCombatCharacter" ), XorStr( "m_hMyWeapons" ) ) / 2;
		Displacement.DT_BaseCombatCharacter.m_hMyWearables = pPropManager->GetOffset( XorStr( "DT_BaseCombatCharacter" ), XorStr( "m_hMyWearables" ) );

		Displacement.C_BasePlayer.m_pCurrentCommand = 0x3338;
		auto relative_call = Memory::Scan( image_client, XorStr( "E8 ? ? ? ? 83 7D D8 00 7C 0F" ) );
		auto offset = *( uintptr_t* )( relative_call + 0x1 );
		Displacement.C_BasePlayer.UpdateVisibilityAllEntities = ( DWORD32 )( relative_call + 5 + offset );

		Displacement.DT_BasePlayer.m_aimPunchAngle = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_aimPunchAngle" ) );
		Displacement.DT_BasePlayer.m_aimPunchAngleVel = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_aimPunchAngleVel" ) );
		Displacement.DT_BasePlayer.m_viewPunchAngle = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_viewPunchAngle" ) );
		Displacement.DT_BasePlayer.m_vecViewOffset = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_vecViewOffset[0]" ) );
		Displacement.DT_BasePlayer.m_vecVelocity = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_vecVelocity[0]" ) );
		Displacement.DT_BasePlayer.m_vecBaseVelocity = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_vecBaseVelocity" ) );
		Displacement.DT_BasePlayer.m_flFallVelocity = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_flFallVelocity" ) );
		Displacement.DT_BasePlayer.m_flDuckAmount = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_flDuckAmount" ) );
		Displacement.DT_BasePlayer.m_flDuckSpeed = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_flDuckSpeed" ) );
		Displacement.DT_BasePlayer.m_lifeState = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_lifeState" ) );
		Displacement.DT_BasePlayer.m_nTickBase = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_nTickBase" ) );
		Displacement.DT_BasePlayer.m_iHealth = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_iHealth" ) );
		Displacement.DT_BasePlayer.m_iDefaultFOV = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_iDefaultFOV" ) );
		Displacement.DT_BasePlayer.m_fFlags = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_fFlags" ) );
		Displacement.DT_BasePlayer.m_bSpotted = pPropManager->GetOffset(XorStr("DT_BasePlayer"), XorStr("m_bSpotted"));
		Displacement.DT_BasePlayer.m_iObserverMode = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_iObserverMode" ) );
		Displacement.DT_BasePlayer.pl = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "pl" ) );
		Displacement.DT_BasePlayer.m_hObserverTarget = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_hObserverTarget" ) );
		Displacement.DT_BasePlayer.m_hViewModel = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_hViewModel[0]" ) );
		Displacement.DT_BasePlayer.m_vphysicsCollisionState = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_vphysicsCollisionState" ) );
		Displacement.DT_BasePlayer.m_ubEFNoInterpParity = pPropManager->GetOffset( XorStr( "DT_BasePlayer" ), XorStr( "m_ubEFNoInterpParity" ) );
		Displacement.DT_BasePlayer.m_ubOldEFNoInterpParity = *( int* )( Memory::Scan( image_client, XorStr( "8A 87 ?? ?? ?? ?? 8D 5F F8" ) ) + 2 ) + 8;

		Displacement.C_CSPlayer.m_PlayerAnimState = *( int* )( Memory::Scan( image_client, XorStr( "8B 8E ?? ?? ?? ?? 85 C9 74 3E" ) ) + 2 );
		Displacement.C_CSPlayer.m_flSpawnTime = *( int* )( Memory::Scan( image_client, XorStr( "89 86 ?? ?? ?? ?? E8 ?? ?? ?? ?? 80 BE ?? ?? ?? ?? ??" ) ) + 2 );
		Displacement.DT_CSPlayer.m_flLowerBodyYawTargetProxy = Memory::Scan( image_engine, XorStr( "EB 0D FF 77 10" ) );

		Displacement.DT_CSPlayer.m_angEyeAngles = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_angEyeAngles[0]" ) );
		Displacement.DT_CSPlayer.m_nSurvivalTeam = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_nSurvivalTeam" ) );
		Displacement.DT_CSPlayer.m_bHasHelmet = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bHasHelmet" ) );
		Displacement.DT_CSPlayer.m_bHasHeavyArmor = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bHasHeavyArmor" ) );
		Displacement.DT_CSPlayer.m_ArmorValue = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_ArmorValue" ) );
		Displacement.DT_CSPlayer.m_bScoped = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bIsScoped" ) );
		Displacement.DT_CSPlayer.m_bIsWalking = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bIsWalking" ) );
		Displacement.DT_CSPlayer.m_iAccount = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_iAccount" ) );
		Displacement.DT_CSPlayer.m_iShotsFired = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_iShotsFired" ) );
		Displacement.DT_CSPlayer.m_flFlashDuration = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_flFlashDuration" ) );
		Displacement.DT_CSPlayer.m_flLowerBodyYawTarget = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_flLowerBodyYawTarget" ) );
		Displacement.DT_CSPlayer.m_flVelocityModifier = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_flVelocityModifier" ) );
		Displacement.DT_CSPlayer.m_bGunGameImmunity = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bGunGameImmunity" ) );
		Displacement.DT_CSPlayer.m_flHealthShotBoostExpirationTime = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_flHealthShotBoostExpirationTime" ) );
		Displacement.DT_CSPlayer.m_iMatchStats_Kills = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_iMatchStats_Kills" ) );
		Displacement.DT_CSPlayer.m_iMatchStats_Deaths = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_iMatchStats_Deaths" ) );
		Displacement.DT_CSPlayer.m_iMatchStats_HeadShotKills = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_iMatchStats_HeadShotKills" ) );
		Displacement.DT_CSPlayer.m_iMoveState = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_iMoveState" ) );
		Displacement.DT_CSPlayer.m_bWaitForNoAttack = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bWaitForNoAttack" ) );
		Displacement.DT_CSPlayer.m_bCustomPlayer = *( int* )( Memory::Scan( image_client, XorStr( "80 BF ?? ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 83 BF ?? ?? ?? ?? ?? 74 7C" ) ) + 2 );
		Displacement.DT_CSPlayer.m_iPlayerState = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_iPlayerState" ) );
		Displacement.DT_CSPlayer.m_bIsDefusing = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bIsDefusing" ) );
		Displacement.DT_CSPlayer.m_bHasDefuser = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bHasDefuser" ) );
		Displacement.DT_CSPlayer.m_iFOV = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_iFOV" ) );
		Displacement.DT_CSPlayer.m_bIsPlayerGhost = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_bIsPlayerGhost" ) );
		Displacement.DT_CSPlayer.m_vecPlayerPatchEconIndices = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_vecPlayerPatchEconIndices" ) );
		Displacement.DT_CSPlayer.m_hRagdoll = pPropManager->GetOffset( XorStr( "DT_CSPlayer" ), XorStr( "m_hRagdoll" ) );

		Displacement.DT_CSRagdoll.m_hPlayer = pPropManager->GetOffset( XorStr( "DT_CSRagdoll" ), XorStr( "m_hPlayer" ) );

		Displacement.DT_FogController.m_fog_enable = pPropManager->GetOffset( XorStr( "DT_FogController" ), XorStr( "m_fog.enable" ) );
		Displacement.DT_Precipitation.m_nPrecipType = pPropManager->GetOffset( XorStr( "DT_Precipitation" ), XorStr( "m_nPrecipType" ) );

		Displacement.DT_BaseCombatWeapon.m_flNextPrimaryAttack = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_flNextPrimaryAttack" ) );
		Displacement.DT_BaseCombatWeapon.m_flNextSecondaryAttack = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_flNextSecondaryAttack" ) );
		Displacement.DT_BaseCombatWeapon.m_hOwner = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_hOwner" ) );
		Displacement.DT_BaseCombatWeapon.m_iClip1 = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_iClip1" ) );
		Displacement.DT_BaseCombatWeapon.m_iPrimaryReserveAmmoCount = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_iPrimaryReserveAmmoCount" ) );
		Displacement.DT_BaseCombatWeapon.m_iItemDefinitionIndex = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_iItemDefinitionIndex" ) );
		Displacement.DT_BaseCombatWeapon.m_hWeaponWorldModel = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_hWeaponWorldModel" ) );
		Displacement.DT_BaseCombatWeapon.m_iWorldModelIndex = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_iWorldModelIndex" ) );
		Displacement.DT_BaseCombatWeapon.m_iWorldDroppedModelIndex = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_iWorldDroppedModelIndex" ) );
		Displacement.DT_BaseCombatWeapon.m_iViewModelIndex = pPropManager->GetOffset( XorStr( "DT_BaseCombatWeapon" ), XorStr( "m_iViewModelIndex" ) );

		Displacement.DT_BaseCombatWeapon.m_CustomMaterials = ( *( int* )( Memory::Scan( image_client, XorStr( "83 BE ? ? ? ? ? 7F 67" ) ) + 0x2 ) ) - 12;
		Displacement.DT_BaseCombatWeapon.m_bCustomMaterialInitialized = *( int* )( Memory::Scan( image_client, XorStr( "C6 86 ? ? ? ? ? FF 50 04" ) ) + 0x2 );

		Displacement.DT_WeaponCSBase.m_flRecoilIndex = pPropManager->GetOffset( XorStr( "DT_WeaponCSBase" ), XorStr( "m_flRecoilIndex" ) );
		Displacement.DT_WeaponCSBase.m_weaponMode = pPropManager->GetOffset( XorStr( "DT_WeaponCSBase" ), XorStr( "m_weaponMode" ) );
		Displacement.DT_WeaponCSBase.m_flPostponeFireReadyTime = pPropManager->GetOffset( XorStr( "DT_WeaponCSBase" ), XorStr( "m_flPostponeFireReadyTime" ) );
		Displacement.DT_WeaponCSBase.m_fLastShotTime = pPropManager->GetOffset( XorStr( "DT_WeaponCSBase" ), XorStr( "m_fLastShotTime" ) );

		Displacement.DT_WeaponCSBaseGun.m_zoomLevel = pPropManager->GetOffset( XorStr( "DT_WeaponCSBaseGun" ), XorStr( "m_zoomLevel" ) );
		Displacement.DT_WeaponCSBaseGun.m_iBurstShotsRemaining = pPropManager->GetOffset( XorStr( "DT_WeaponCSBaseGun" ), XorStr( "m_iBurstShotsRemaining" ) );
		Displacement.DT_WeaponCSBaseGun.m_fNextBurstShot = pPropManager->GetOffset( XorStr( "DT_WeaponCSBaseGun" ), XorStr( "m_fNextBurstShot" ) );

		Displacement.DT_BaseCSGrenade.m_bPinPulled = pPropManager->GetOffset( XorStr( "DT_BaseCSGrenade" ), XorStr( "m_bPinPulled" ) );
		Displacement.DT_BaseCSGrenade.m_fThrowTime = pPropManager->GetOffset( XorStr( "DT_BaseCSGrenade" ), XorStr( "m_fThrowTime" ) );
		Displacement.DT_BaseCSGrenade.m_flThrowStrength = pPropManager->GetOffset( XorStr( "DT_BaseCSGrenade" ), XorStr( "m_flThrowStrength" ) );

		Displacement.DT_BaseAttributableItem.m_flFallbackWear = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_flFallbackWear" ) );
		Displacement.DT_BaseAttributableItem.m_nFallbackPaintKit = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_nFallbackPaintKit" ) );
		Displacement.DT_BaseAttributableItem.m_nFallbackSeed = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_nFallbackSeed" ) );
		Displacement.DT_BaseAttributableItem.m_nFallbackStatTrak = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_nFallbackStatTrak" ) );
		Displacement.DT_BaseAttributableItem.m_OriginalOwnerXuidHigh = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_OriginalOwnerXuidHigh" ) );
		Displacement.DT_BaseAttributableItem.m_OriginalOwnerXuidLow = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_OriginalOwnerXuidLow" ) );
		Displacement.DT_BaseAttributableItem.m_szCustomName = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_szCustomName" ) );
		Displacement.DT_BaseAttributableItem.m_bInitialized = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_bInitialized" ) );
		Displacement.DT_BaseAttributableItem.m_iAccountID = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_iAccountID" ) );
		Displacement.DT_BaseAttributableItem.m_iEntityLevel = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_iEntityLevel" ) );
		Displacement.DT_BaseAttributableItem.m_iEntityQuality = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_iEntityQuality" ) );
		Displacement.DT_BaseAttributableItem.m_iItemDefinitionIndex = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_iItemDefinitionIndex" ) );
		Displacement.DT_BaseAttributableItem.m_Item = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_Item" ) );
		Displacement.DT_BaseAttributableItem.m_iItemIDLow = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_iItemIDLow" ) );
		Displacement.DT_BaseAttributableItem.m_iItemIDHigh = pPropManager->GetOffset( XorStr( "DT_BaseAttributableItem" ), XorStr( "m_iItemIDHigh" ) );

		Displacement.DT_BaseViewModel.m_hOwner = pPropManager->GetOffset( XorStr( "DT_BaseViewModel" ), XorStr( "m_hOwner" ) );
		Displacement.DT_BaseViewModel.m_hWeapon = pPropManager->GetOffset( XorStr( "DT_BaseViewModel" ), XorStr( "m_hWeapon" ) );
		Displacement.DT_BaseViewModel.m_nSequence = pPropManager->GetOffset( XorStr( "DT_BaseViewModel" ), XorStr( "m_nSequence" ) );

		Displacement.DT_SmokeGrenadeProjectile.m_nSmokeEffectTickBegin = pPropManager->GetOffset( XorStr( "DT_SmokeGrenadeProjectile" ), XorStr( "m_bDidSmokeEffect" ) );
		Displacement.DT_SmokeGrenadeProjectile.m_nSmokeCount = Memory::Scan( image_client, XorStr( "A3 ? ? ? ? 57 8B CB" ) ) + 0x1;
		//Displacement.DT_SmokeGrenadeProjectile.m_SmokeParticlesSpawned = *( int* )( Memory::Scan( image_client, XorStr( "80 BF ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? F3 0F 7E 87 ?? ?? ?? ??" ) ) + 2 );

		Displacement.CBoneMergeCache.m_nConstructor = Memory::Scan( image_client, XorStr( "56 8B F1 0F 57 C0 C7 86 ?? ?? ?? ?? ?? ?? ?? ??" ) );
		Displacement.CBoneMergeCache.m_nInit = CallableFromRelative( Memory::Scan( image_client, XorStr( "E8 ?? ?? ?? ?? FF 75 08 8B 8E ?? ?? ?? ??" ) ) );
		Displacement.CBoneMergeCache.m_nUpdateCache = CallableFromRelative( Memory::Scan( image_client, XorStr( "E8 ?? ?? ?? ?? 83 7E 10 00 74 64" ) ) );
		Displacement.CBoneMergeCache.m_CopyToFollow = CallableFromRelative( Memory::Scan( image_client, XorStr( "E8 ?? ?? ?? ?? 8B 87 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 8B 7C 24 18" ) ) );
		Displacement.CBoneMergeCache.m_CopyFromFollow = CallableFromRelative( Memory::Scan( image_client, XorStr( "E8 ?? ?? ?? ?? F3 0F 10 45 ?? 8D 84 24 ?? ?? ?? ??" ) ) );

		//	Displacement.CIKContext.m_nConstructor = Memory::Scan( image_client, XorStr( "53 8B D9 F6 C3 03 74 0B FF 15 ?? ?? ?? ?? 84 C0 74 01 CC C7 83 ?? ?? ?? ?? ?? ?? ?? ?? 8B CB" ) );
		Displacement.CIKContext.m_nDestructor = Memory::Scan( image_client, XorStr( "56 8B F1 57 8D 8E ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 8E ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 BE ?? ?? ?? ?? ??" ) );
		Displacement.CIKContext.m_nInit = Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 08 8B 45 08 56 57 8B F9 8D 8F" ) );
		Displacement.CIKContext.m_nUpdateTargets = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F0 81 EC ?? ?? ?? ?? 33 D2" ) );
		Displacement.CIKContext.m_nSolveDependencies = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F0 81 EC ?? ?? ?? ?? 8B 81" ) );

		Displacement.CBoneSetup.InitPose = Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 10 53 8B D9 89 55 F8 56 57 89 5D F4 8B 0B 89 4D F0" ) );
		Displacement.CBoneSetup.AccumulatePose = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F0 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ??" ) );
		Displacement.CBoneSetup.CalcAutoplaySequences = Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 10 53 56 57 8B 7D 10 8B D9 F3 0F 11 5D ??" ) );
		Displacement.CBoneSetup.CalcBoneAdj = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F8 81 EC ?? ?? ?? ?? 8B C1 89 54 24 04 89 44 24 2C 56 57 8B 00" ) );

		Displacement.IPrediction.m_nCommandsPredicted = 0x1C;

		// jmp patterns is not very reliable
		auto CL_Predict = Memory::Scan( image_engine, XorStr( "75 30 8B 87 ?? ?? ?? ??" ) );
		auto CL_Move = Memory::Scan( image_engine, XorStr( "74 0F 80 BF ?? ?? ?? ?? ??" ) );
		Displacement.CClientState.m_nLastCommandAck = *( int* )( CL_Predict + 0x20 );
		Displacement.CClientState.m_nDeltaTick = *( int* )( CL_Predict + 0x10 );
		Displacement.CClientState.m_nLastOutgoingCommand = *( int* )( CL_Predict + 0xA );
		Displacement.CClientState.m_nChokedCommands = *( int* )( CL_Predict + 0x4 );
		Displacement.CClientState.m_bIsHLTV = *( int* )( CL_Move + 0x4 );

		Displacement.DT_PlantedC4.m_flC4Blow = pPropManager->GetOffset( XorStr( "DT_PlantedC4" ), XorStr( "m_flC4Blow" ) );

		Displacement.Data.m_uMoveHelper = **( std::uintptr_t** )( Memory::Scan( image_client, XorStr( "8B 0D ?? ?? ?? ?? 8B 46 08 68" ) ) + 2 );
		Displacement.Data.m_uInput = *( std::uintptr_t* )( Memory::Scan( image_client, XorStr( "B9 ?? ?? ?? ?? F3 0F 11 04 24 FF 50 10" ) ) + 1 );
		Displacement.Data.m_uPredictionRandomSeed = *( std::uintptr_t* )( Memory::Scan( image_client, XorStr( "8B 0D ?? ?? ?? ?? BA ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4" ) ) + 2 );
		Displacement.Data.m_uPredictionPlayer = *( std::uintptr_t* )( Memory::Scan( image_client, XorStr( "89 ?? ?? ?? ?? ?? F3 0F 10 48 20" ) ) + 2 );
		Displacement.Data.m_uModelBoneCounter = *( std::uintptr_t* )( Displacement.C_BaseAnimating.InvalidateBoneCache + 0xA );
		Displacement.Data.m_uClientSideAnimationList = *( std::uintptr_t* )( Memory::Scan( image_client, XorStr( "A1 ?? ?? ?? ?? F6 44 F0 04 01 74 0B" ) ) + 1 );
		Displacement.Data.m_uGlowObjectManager = *( std::uintptr_t* )( Memory::Scan( image_client, XorStr( "0F 11 05 ?? ?? ?? ?? 83 C8 01" ) ) + 3 );
		Displacement.Data.m_uCamThink = ( std::uintptr_t )( Memory::Scan( image_client, XorStr( "85 C0 75 30 38 86" ) ) );
		Displacement.Data.m_uRenderBeams = ( std::uintptr_t )( Memory::Scan( image_client, XorStr( "A1 ?? ?? ?? ?? FF 10 A1 ?? ?? ?? ?? B9" ) ) + 0x1 );
		//Displacement.Data.m_uSmokeCount = *( std::uintptr_t* )( Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 08 8B 15 ?? ?? ?? ?? 0F 57 C0" ) ) + 0x8 );
		Displacement.Data.m_uCenterPrint = ( std::uintptr_t )( Memory::Scan( image_client, XorStr( "8B 35 ? ? ? ? 8D 4C 24 20" ) ) + 0x2 );
		Displacement.Data.m_uHostFrameTicks = ( Memory::Scan( image_engine, XorStr( "03 05 ? ? ? ? 83 CF 10" ) ) + 2 );
		Displacement.Data.m_uServerGlobals = Memory::Scan( image_server, XorStr( "8B 15 ? ? ? ? 33 C9 83 7A 18 01" ) ) + 0x2;
		Displacement.Data.m_uServerPoseParameters = Memory::Scan( image_server, XorStr( "8D 87 ? ? ? ? 89 46 08 C7 46 ? ? ? ? ? EB 06" ) ) + 0x2;
		Displacement.Data.m_uServerAnimState = Memory::Scan( image_server, XorStr( "8B 8F ?? ?? ?? ?? 85 C9 74 06 56" ) ) + 2;
		Displacement.Data.m_uTicksAllowed = Memory::Scan( image_server, XorStr( "FF 86 ?? ?? ?? ?? 8B CE 89 86 ?? ?? ?? ??" ) ) + 2;
		Displacement.Data.m_uHudElement = Memory::Scan( image_client, XorStr( "B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 5D 08" ) ) + 1;
		//Displacement.Data.m_uListLeavesInBoxReturn = Memory::Scan( image_client, XorStr( "56 52 FF 50 18" ) ) + 0x5;
		Displacement.Data.s_bAllowExtrapolation = Memory::Scan( image_client, XorStr( "A2 ? ? ? ? 8B 45 E8" ) ) + 1;
		Displacement.Data.m_FireBulletsReturn = Memory::Scan( image_client, XorStr( "3B 3D ?? ?? ?? ?? 75 4C" ) );
		Displacement.Data.m_D3DDevice = Memory::Scan( image_shaderapidx9, XorStr( "A1 ?? ?? ?? ?? 50 8B 08 FF 51 0C" ) ) + 1;
		Displacement.Data.m_SoundService = Memory::Scan( image_engine, XorStr( "B9 ? ? ? ? 80 65 FC FE 6A 00" ) );
		Displacement.Data.m_InterpolateServerEntities = Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 1C 8B 0D ? ? ? ? 53 56" ) ); // xref CVProfile::EnterScope(g_VProfCurrentProfile, ("C_BaseEntity::InterpolateServerEntities"), 0, ("Interpolation"), 0, 4);
		Displacement.Data.m_SendNetMsg = Memory::Scan( image_engine, XorStr( "55 8B EC 56 8B F1 8B 86 ? ? ? ? 85 C0 74 24 48 83 F8 02 77 2C 83 BE ? ? ? ? ? 8D 8E ? ? ? ? 74 06 32 C0 84 C0 EB 10 E8 ? ? ? ? 84 C0 EB 07 83 BE ? ? ? ? ? 0F 94 C0 84 C0 74 07 B0 01 5E 5D C2 0C 00" ) ); // xref volume || ConVarRef %s doesn't point to an existing ConVar\n
		Displacement.Data.m_ModifyEyePos = CallableFromRelative( Memory::Scan( image_client, XorStr( "E8 ? ? ? ? 8B 06 8B CE FF 90 ? ? ? ? 85 C0 74 4E" ) ) ); // xref head_0
		Displacement.Data.m_ResetContentsCache = Memory::Scan( image_client, XorStr( "56 8D 51 3C BE" ) );
		Displacement.Data.m_ProcessInterpolatedList = Memory::Scan( image_client, XorStr( "0F B7 05 ? ? ? ? 3D ? ? ? ? 74 3F" ) ); // xref C_BaseEntity::InterpolateServerEntities
		Displacement.Data.CheckReceivingListReturn = *reinterpret_cast< DWORD32* >( Memory::Scan( image_engine, XorStr( "FF 50 34 8B 1D ? ? ? ? 85 C0 74 16 FF B6" ) ) + 0x3 );
		Displacement.Data.ReadSubChannelDataReturn = *reinterpret_cast< DWORD32* >( Memory::Scan( image_engine, XorStr( "FF 50 34 85 C0 74 12 53 FF 75 0C 68" ) ) + 0x3 );
		Displacement.Data.SendDatagram = Memory::Scan( image_engine, XorStr( "55 8B EC 83 E4 F0 B8 ? ? ? ? E8 ? ? ? ? 56 57 8B F9 89 7C 24 18" ) );
		Displacement.Data.ProcessPacket = Memory::Scan( image_engine, XorStr( "55 8B EC 83 E4 C0 81 EC ? ? ? ? 53 56 57 8B 7D 08 8B D9" ) );
		Displacement.Data.m_GameRules = ( Memory::Scan( image_client, XorStr( "8B 0D ?? ?? ?? ?? 85 C0 74 0A 8B 01 FF 50 78 83 C0 54" ) ) + 2 );

		Displacement.Function.m_uRandomSeed = ( std::uintptr_t )( GetProcAddress( image_vstdlib, XorStr( "RandomSeed" ) ) );
		Displacement.Function.m_uRandomFloat = ( std::uintptr_t )( GetProcAddress( image_vstdlib, XorStr( "RandomFloat" ) ) );
		Displacement.Function.m_uRandomInt = ( std::uintptr_t )( GetProcAddress( image_vstdlib, XorStr( "RandomInt" ) ) );

		Displacement.Function.m_uSetAbsOrigin = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F8 51 53 56 57 8B F1 E8" ) );
		Displacement.Function.m_uSetAbsAngles = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F8 83 EC 64 53 56 57 8B F1 E8" ) );
		Displacement.Function.m_uIsBreakable = Memory::Scan( image_client, XorStr( "55 8B EC 51 56 8B F1 85 F6 74 68" ) ); //xref
		//Displacement.Function.m_uClearHudWeaponIcon = Memory::Scan( image_client, XorStr( "55 8B EC 51 53 56 8B 75 08 8B D9 57 6B FE 2C 89 5D FC" ) );
		Displacement.Function.m_uLoadNamedSkys = Memory::Scan( image_engine, XorStr( "55 8B EC 81 EC ? ? ? ? 56 57 8B F9 C7 45" ) );
		Displacement.Function.m_uCreateAnimState = Memory::Scan( image_client, XorStr( "55 8B EC 56 8B F1 B9 ?? ?? ?? ?? C7 46" ) );
		Displacement.Function.m_uResetAnimState = Memory::Scan( image_client, XorStr( "56 6A 01 68 ?? ?? ?? ?? 8B F1" ) );
		Displacement.Function.m_uUpdateAnimState = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F8 83 EC 18 56 57 8B F9 F3 0F 11 54 24" ) );
		Displacement.Function.m_uClanTagChange = Memory::Scan( image_engine, XorStr( "53 56 57 8B DA 8B F9 FF 15" ) );
		Displacement.Function.m_uGetSequenceActivity = Memory::Scan( image_client, XorStr( "55 8B EC 83 7D 08 FF 56 8B F1 74 3D" ) );
		Displacement.Function.m_uInvalidatePhysics = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F8 83 EC 0C 53 8B 5D 08 8B C3 56 83 E0 04" ) );
		Displacement.Function.m_uPostThinkVPhysics = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F8 81 EC ? ? ? ? 53 8B D9 56 57 83 BB ? ? ? ? ? 75 50 8B 0D" ) );
		Displacement.Function.m_SimulatePlayerSimulatedEntities = Memory::Scan( image_client, XorStr( "56 8B F1 57 8B BE ?? ?? ?? ?? 83 EF 01 78 72 90 8B 86" ) );
		Displacement.Function.m_uImplPhysicsRunThink = Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 10 53 56 57 8B F9 8B 87 ?? ?? ?? ?? C1 E8 16" ) );
		//Displacement.Function.m_uClearDeathNotices = Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 0C 53 56 8B 71 58" ) ); //55 8B EC 83 EC 0C 53 56 8B 71 58 33 DB 57 89 5D F8 8B 4E 04 8B 01 FF 90
		Displacement.Function.m_uSetTimeout = Memory::Scan( image_engine, XorStr( "55 8B EC 80 7D 0C 00 F3 0F 10 4D" ) );
		Displacement.Function.m_uFindHudElement = Memory::Scan( image_client, XorStr( "55 8B EC 53 8B 5D 08 56 57 8B F9 33 F6 39 77 28" ) );
		Displacement.Function.m_SetCollisionBounds = Memory::Scan( image_client, XorStr( "53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 83 EC 10 56 57 8B 7B" ) );
		Displacement.Function.m_MD5PseudoRandom = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F8 83 EC 70 6A 58 8D 44 24 1C 89 4C 24 08 6A 00 50" ) );
		Displacement.Function.m_WriteUsercmd = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F8 51 53 56 8B D9 8B 0D" ) );
		Displacement.Function.m_StdStringAssign = Memory::Scan( image_engine, XorStr( "55 8B EC 53 8B 5D 08 56 8B F1 85 DB 74 57 8B 4E 14 83 F9 10 72 04 8B 06 EB 02" ) );
		Displacement.Function.m_pPoseParameter = Memory::Scan( image_client, XorStr( "55 8B EC 8B 45 08 57 8B F9 8B 4F 04 85 C9 75 15 8B" ) );
		Displacement.Function.m_AttachmentHelper = Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 48 53 8B 5D 08 89 4D F4" ) );
		Displacement.Function.m_LockStudioHdr = CallableFromRelative( Memory::Scan( image_client, XorStr( "E8 ?? ?? ?? ?? 8D 47 FC" ) ) );
		Displacement.Function.m_LineGoesThroughSmoke = Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 08 8B 15 ?? ?? ?? ?? 0F 57 C0" ) );
		Displacement.Function.m_RunSimulation = CallableFromRelative( Memory::Scan( image_client, XorStr( "E8 ? ? ? ? A1 ? ? ? ? F3 0F 10 45 ? F3 0F 11 40" ) ) );
		Displacement.Function.m_PredictionUpdate = Memory::Scan( image_client, XorStr( "55 8B EC 83 EC 08 53 56 8B F1 8B 0D ? ? ? ? 57 8B B9" ) );
		Displacement.Function.m_TraceFilterSimple = Memory::Scan( image_client, XorStr( "55 8B EC 83 E4 F0 83 EC 7C 56 52" ) ) + 0x3D; //xref : offset ??_7CTraceFilterSimple@@6B@ ; const CTraceFilterSimple::`vftable' 
	}

	bool CreateDisplacement( void* reserved ) {
		Create( );

		//  DumpOffsets( reserved );
		return true;
	}
}