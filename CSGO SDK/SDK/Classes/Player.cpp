#include "player.hpp"
#include "../displacement.hpp"
#include "../source.hpp"
#include "weapon.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"

QAngle& C_BasePlayer::m_aimPunchAngle( ) {
	return *( QAngle* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_aimPunchAngle );
}

QAngle& C_BasePlayer::m_aimPunchAngleVel( ) {
	return *( QAngle* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_aimPunchAngleVel );
}

QAngle& C_BasePlayer::m_viewPunchAngle( ) {
	return *( QAngle* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_viewPunchAngle );
}

Vector& C_BasePlayer::m_vecViewOffset( ) {
	return *( Vector* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_vecViewOffset );
}
Vector& C_BasePlayer::m_vecVelocity( ) {
	return *( Vector* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_vecVelocity );
}

Vector& C_BasePlayer::m_vecBaseVelocity( ) {
	return *( Vector* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_vecBaseVelocity );
}

float& C_BasePlayer::m_flFallVelocity( ) {
	return *( float* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_flFallVelocity );
}

float& C_BasePlayer::m_flDuckAmount( ) {
	return *( float* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_flDuckAmount );
}

float& C_BasePlayer::m_flDuckSpeed( ) {
	return *( float* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_flDuckSpeed );
}

char& C_BasePlayer::m_lifeState( ) {
	return *( char* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_lifeState );
}

int& C_BasePlayer::m_nTickBase( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_nTickBase );
}

int& C_BasePlayer::m_iHealth( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_iHealth );
}

int& C_BasePlayer::m_fFlags( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_fFlags );
}

int& C_BasePlayer::m_iDefaultFOV( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_iDefaultFOV );
}

int& C_BasePlayer::m_iObserverMode( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_iObserverMode );
}

bool& C_BasePlayer::m_bSpotted() {
	return *(bool*)((uintptr_t)this + Engine::Displacement.DT_BasePlayer.m_bSpotted);
}

CPlayerState& C_BasePlayer::pl( ) {
	return *( CPlayerState* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.pl );
}

CBaseHandle& C_BasePlayer::m_hObserverTarget( ) {
	return *( CBaseHandle* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_hObserverTarget );
}

CHandle<C_BaseViewModel> C_BasePlayer::m_hViewModel( ) {
	return *( CHandle<C_BaseViewModel>* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_hViewModel );
}

int& C_BasePlayer::m_vphysicsCollisionState( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_BasePlayer.m_vphysicsCollisionState );
}

float C_BasePlayer::GetMaxSpeed( ) {
	if( !this )
		return 250.0f;

	auto hWeapon = this->m_hActiveWeapon( );
	if( hWeapon == -1 )
		return 250.0f;

	auto pWeapon = reinterpret_cast< C_WeaponCSBaseGun* >( this->m_hActiveWeapon( ).Get( ) );
	if( !pWeapon )
		return 250.0f;

	auto pWeaponData = pWeapon->GetCSWeaponData( );
	if( !pWeaponData.IsValid( ) )
		return 250.0f;

	if( pWeapon->m_zoomLevel( ) == 0 )
		return pWeaponData->m_flMaxSpeed;

	return pWeaponData->m_flMaxSpeed2;
}


float C_BasePlayer::SequenceDuration( CStudioHdr* pStudioHdr, int iSequence ) {
	//55 8B EC 53 57 8B 7D 08 8B D9 85 FF 75 double __userpurge SequenceDuration@<st0>(int a1@<ecx>, float a2@<xmm0>, int *a3, int a4)
	using SequenceDurationFn = float( __thiscall* )( void*, CStudioHdr*, int );
	return Memory::VCall< SequenceDurationFn >( this, 221 )( this, pStudioHdr, iSequence );
}

const Vector& C_BasePlayer::WorldSpaceCenter( ) {
	using WorldSpaceCenterFn = const Vector& ( __thiscall* )( void* );
	return Memory::VCall< WorldSpaceCenterFn >( this, 78 )( this );
}

float C_BasePlayer::GetSequenceMoveDist( CStudioHdr* pStudioHdr, int iSequence ) {
	Vector vecReturn;

	auto rel_32_fix = [ ] ( uintptr_t ptr ) -> uintptr_t { // TODO: Move this to displacement
		auto offset = *( uintptr_t* )( ptr + 0x1 );
		return ( uintptr_t )( ptr + 5 + offset );
	};

	// int __usercall GetSequenceLinearMotion@<eax>(int a1@<edx>, _DWORD *a2@<ecx>, int a3, _DWORD *a4)
	// it fastcall, but edx and ecx swaped
	// xref: Bad pstudiohdr in GetSequenceLinearMotion()!\n | Bad sequence (%i out of %i max) in GetSequenceLinearMotion() for model '%s'!\n
	static uintptr_t ptr = rel_32_fix( Memory::Scan( XorStr( "client.dll" ), XorStr( "E8 ? ? ? ? F3 0F 10 4D ? 83 C4 08 F3 0F 10 45 ? F3 0F 59 C0" ) ) );


	using GetSequenceLinearMotionFn = int( __fastcall* )( CStudioHdr*, int, float*, Vector* );
	( ( GetSequenceLinearMotionFn )ptr )( pStudioHdr, iSequence, m_flPoseParameter( ), &vecReturn );
	__asm {
		add esp, 8
	}
	return vecReturn.Length( );
}

bool C_BasePlayer::IsDead( ) {
	return ( this->m_lifeState( ) );
}

void C_BasePlayer::SetCurrentCommand( CUserCmd* cmd ) {
	*( CUserCmd** )( ( uintptr_t )this + Engine::Displacement.C_BasePlayer.m_pCurrentCommand ) = cmd;
}

Vector C_BasePlayer::GetEyePosition( ) {
	Vector vecOrigin = this->GetAbsOrigin( );

	Vector offset = m_vecViewOffset( );
	if( offset.z >= 46.1f ) {
		if( offset.z > 64.0f ) {
			offset.z = 64.0f;
		}
	}
	else {
		offset.z = 46.0f;
	}
	vecOrigin += offset;

	return vecOrigin;
}

Vector C_BasePlayer::GetViewHeight( ) {
	Vector offset;
	if( this->m_flDuckAmount( ) == 0.0f ) {
		offset = Interfaces::m_pGameMovement->GetPlayerViewOffset( false );
	}
	else {
		offset = m_vecViewOffset( );
	}
	return offset;
}

float C_BasePlayer::GetLayerSequenceCycleRate( C_AnimationLayer* pLayer, int iSequence ) {
	using GetLayerSequenceCycleRateFn = float( __thiscall* )( void*, C_AnimationLayer*, int );
	return Memory::VCall< GetLayerSequenceCycleRateFn >( this, 222 )( this, pLayer, iSequence );
}

C_AnimationLayer& C_BasePlayer::GetAnimLayer( int index ) {
	// ref: CCBaseEntityAnimState::ComputePoseParam_MoveYaw
	// move_x move_y move_yaw
	typedef C_AnimationLayer& ( __thiscall* Fn )( void*, int, bool );
	static Fn fn = NULL;

	if( !fn )
		fn = ( Fn )Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 57 8B F9 8B 97 ? ? ? ? 85 D2" ) );

	index = Math::Clamp( index, 0, 13 );

	return fn( this, index, true );
}

// L3D451R7, raxer23 (c)
void C_BasePlayer::TryInitiateAnimation( C_AnimationLayer* pLayer, int nSequence ) {
	if( !pLayer || nSequence < 2 )
		return;

	pLayer->m_flPlaybackRate = GetLayerSequenceCycleRate( pLayer, nSequence );
	pLayer->m_nSequence = nSequence;
	pLayer->m_flCycle = pLayer->m_flWeight = 0.f;
};

C_CSPlayer* C_CSPlayer::GetLocalPlayer( ) {
	auto index = Interfaces::m_pEngine->GetLocalPlayer( );

	if( !index )
		return nullptr;

	auto client = Interfaces::m_pEntList->GetClientEntity( index );

	if( !client )
		return nullptr;

	return ToCSPlayer( client->GetBaseEntity( ) );
}

C_CSPlayer* C_CSPlayer::GetPlayerByIndex( int index ) {
	if( !index )
		return nullptr;

	auto client = Interfaces::m_pEntList->GetClientEntity( index );

	if( !client )
		return nullptr;

	return ToCSPlayer( client->GetBaseEntity( ) );
}

std::array<int, 5>& C_CSPlayer::m_vecPlayerPatchEconIndices( )
{
	return *( std::array<int, 5>* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_vecPlayerPatchEconIndices );
}

CCSGOPlayerAnimState*& C_CSPlayer::m_PlayerAnimState( ) {
	return *( CCSGOPlayerAnimState** )( ( uintptr_t )this + Engine::Displacement.C_CSPlayer.m_PlayerAnimState );
}

QAngle& C_CSPlayer::m_angEyeAngles( ) {
	return *( QAngle* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_angEyeAngles );
}

int& C_CSPlayer::m_nSurvivalTeam( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_nSurvivalTeam );
}

int& C_CSPlayer::m_ArmorValue( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_ArmorValue );
}

int& C_CSPlayer::m_iAccount( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_iAccount );
}

int& C_CSPlayer::m_iFOV( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_iFOV );
}

int& C_CSPlayer::m_iShotsFired( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_iShotsFired );
}

float& C_CSPlayer::m_flFlashDuration( ) {
	return *( float* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_flFlashDuration );
}

float& C_CSPlayer::m_flSecondFlashAlpha( ) {
	return *( float* )( uintptr_t( this ) + Engine::Displacement.DT_CSPlayer.m_flFlashDuration - 0xC );
}

float& C_CSPlayer::m_flVelocityModifier( ) {
	return *( float* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_flVelocityModifier );
}

float& C_CSPlayer::m_flLowerBodyYawTarget( ) {
	return *( float* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_flLowerBodyYawTarget );
}

float& C_CSPlayer::m_flSpawnTime( ) {
	return *( float* )( ( uintptr_t )this + Engine::Displacement.C_CSPlayer.m_flSpawnTime );
}

float& C_CSPlayer::m_flHealthShotBoostExpirationTime( ) {
	return *( float* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_flHealthShotBoostExpirationTime );
}

bool& C_CSPlayer::m_bHasHelmet( ) {
	return *( bool* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_bHasHelmet );
}

bool& C_CSPlayer::m_bHasHeavyArmor( ) {
	return *( bool* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_bHasHeavyArmor );
}

bool& C_CSPlayer::m_bIsScoped( ) {
	return *( bool* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_bScoped );
}

bool& C_CSPlayer::m_bWaitForNoAttack( ) {
	return *( bool* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_bWaitForNoAttack );
}

bool& C_CSPlayer::m_bIsPlayerGhost( )
{
	return *( bool* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_bIsPlayerGhost );
}

int& C_CSPlayer::m_iMatchStats_Kills( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_iMatchStats_Kills );
}

int& C_CSPlayer::m_iMatchStats_Deaths( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_iMatchStats_Deaths );
}

int& C_CSPlayer::m_iMatchStats_HeadShotKills( ) {
	return *( int* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_iMatchStats_HeadShotKills );
}

bool& C_CSPlayer::m_bGunGameImmunity( ) {
	return *( bool* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_bGunGameImmunity );
}

bool& C_CSPlayer::m_bIsDefusing( ) {
	return *( bool* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_bIsDefusing );
}

bool& C_CSPlayer::m_bHasDefuser( ) {
	return *( bool* )( ( uintptr_t )this + Engine::Displacement.DT_CSPlayer.m_bHasDefuser );
}

bool C_CSPlayer::PhysicsRunThink( int unk01 ) {
	static auto impl_PhysicsRunThink = ( bool( __thiscall* )( void*, int ) )Engine::Displacement.Function.m_uImplPhysicsRunThink;
	return impl_PhysicsRunThink( this, unk01 );
}

int C_CSPlayer::SetNextThink( int tick ) {
	static const auto next_think = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 56 57 8B F9 8B B7 E8" ) );

	typedef int( __thiscall* fnSetNextThink ) ( C_CSPlayer*, int tick );
	auto ret = ( ( fnSetNextThink )next_think ) ( this, tick );
	return ret;
}

void C_CSPlayer::Think( ) {
	static auto index = *( int* )( Memory::Scan( XorStr( "client.dll" ), XorStr( "FF 90 ? ? ? ? FF 35 ? ? ? ? 8B 4C 24 3C" ) ) + 2 ) / 4; // ref: CPrediction::ProcessMovement  (*(void (__thiscall **)(_DWORD *))(*player + 552))(player);
	//xref CPrediction::ProcessMovement (*(void (__thiscall **)(_DWORD *))(*a2 + 552))(a2);
	using Fn = void( __thiscall* )( void* );
	Memory::VCall<Fn>( this, 137 )( this ); // 139
}

void C_CSPlayer::PreThink( ) {
	static auto index = *( int* )( Memory::Scan( XorStr( "client.dll" ), XorStr( "FF 90 ? ? ? ? 8B 86 ? ? ? ? 83 F8 FF" ) ) + 2 ) / 4;
	//xref CPrediction::ProcessMovement 
	// if ( (unsigned __int8)sub_102FED00(0) )
	// (*(void (__thiscall **)(_DWORD *))(*a2 + 1268))(a2);
	using Fn = void( __thiscall* )( void* );
	Memory::VCall<Fn>( this, 307 )( this ); // 169
}

void C_CSPlayer::PostThink( ) {
	using Fn = void( __thiscall* )( void* );
	Memory::VCall<Fn>( this, /*316*/ 308 )( this );
}

bool C_CSPlayer::is( std::string networkname )
{
	auto& propmanager = Engine::PropManager::Instance( );

	auto clientClass = this->GetClientClass( );
	if( !clientClass )
		return false;

	return propmanager.GetClientID( networkname ) == clientClass->m_ClassID;
}

bool C_CSPlayer::IsTeammate( C_CSPlayer* player ) {
	if( !player || !this )
		return false;

	return this->m_iTeamNum( ) == player->m_iTeamNum( );
}

bool C_CSPlayer::CanShoot( bool bSkipRevolver ) {
	bool local = EntIndex( ) == Interfaces::m_pEngine->GetLocalPlayer( );

	auto weapon = ( C_WeaponCSBaseGun* )( this->m_hActiveWeapon( ).Get( ) );
	if( !weapon )
		return false;

	auto weapon_data = weapon->GetCSWeaponData( );
	if( !weapon_data.IsValid( ) )
		return false;

	auto g_GameRules = *( uintptr_t** )( Engine::Displacement.Data.m_GameRules );
	if( !g_GameRules )
		return false;

	if( weapon->m_iItemDefinitionIndex( ) != WEAPON_REVOLVER ) {
		if( g_Vars.globals.WasShootingInChokeCycle || g_Vars.globals.WasShooting && local )
			return false;
	}

	if( this->m_fFlags( ) & 0x40 )
		return false;

	if( *( bool* )( *( uintptr_t* )g_GameRules + 0x20 ) )
		return false;

	if( this->m_bWaitForNoAttack( ) )
		return false;

	if( *( int* )( uintptr_t( this ) + Engine::Displacement.DT_CSPlayer.m_iPlayerState ) )
		return false;

	if( this->m_bIsDefusing() )
		return false;

	if( this->IsReloading( ) )
		return false;

	if( weapon_data->m_iWeaponType >= WEAPONTYPE_PISTOL && weapon_data->m_iWeaponType <= WEAPONTYPE_MACHINEGUN && weapon->m_iClip1( ) < 1 )
		return false;

	float curtime = TICKS_TO_TIME( this->m_nTickBase( ) );
	if( curtime < m_flNextAttack( ) )
		return false;

	if( ( weapon->m_iItemDefinitionIndex( ) == WEAPON_GLOCK || weapon->m_iItemDefinitionIndex( ) == WEAPON_FAMAS ) && weapon->m_iBurstShotsRemaining( ) > 0 ) {
		if( curtime >= weapon->m_fNextBurstShot( ) )
			return true;
	}

	if( curtime < weapon->m_flNextPrimaryAttack( ) )
		return false;

	if( weapon->m_iItemDefinitionIndex( ) != WEAPON_REVOLVER )
		return true;

	if( bSkipRevolver )
		return true;

	if( *( int* )( uintptr_t( weapon ) + Engine::Displacement.DT_BaseAnimating.m_nSequence ) != 5 )
		return false;

	return curtime >= weapon->m_flPostponeFireReadyTime( );
}

bool C_CSPlayer::IsReloading( ) {
	auto animLayer = this->m_AnimOverlay( ).Element( 1 );
	if( !animLayer.m_pOwner )
		return false;

	return GetSequenceActivity( animLayer.m_nSequence ) == 967 && animLayer.m_flWeight != 0.f;
}

void CCSGOPlayerAnimState::ModifyEyePosition( CCSGOPlayerAnimState *pState, Vector *pos ) {
	/* NOTE FROM ALPHA:
	 * Although I had the choice to paste this from DucaRii and be done with it, I wanted to avoid that as much as i possibly can
	 * Tt's super easy to get to this function - throw server.dll into IDA, open strings view and search for "head_0"
	 * find xrefs to it (there should be 3 in total: https://i.imgur.com/n4Uguxo.png) and pick the 3rd one (call to CCSGOPlayerAnimState::ModifyEyePosition)
	 * boom you're here
	 */

	if( !pState || !pState->m_Player ) {
		return;
	}

	// If this sig ever goes out of date, refer to the instructions above to get into here and sig this:
	// https://i.imgur.com/zGnqd3y.png
	static auto C_BaseAnimating__LookupBone = *reinterpret_cast< int( __thiscall * )( void *, const char * ) >( Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 53 56 8B F1 57 83 BE ?? ?? ?? ?? ?? 75 14" ) ) );

	if( pState->m_Player && pState->m_bHitground || pState->m_fDuckAmount != 0.f ) {
		// this returns 8 but i'd rather grab it dynamically in the rare event of it changing
		auto v5 = C_BaseAnimating__LookupBone( pState->m_Player, XorStr( "head_0" ) );

		if( v5 != -1 ) {
			auto v12 = ( reinterpret_cast< C_CSPlayer * >( pState->m_Player ) )->GetBonePos( v5 );
			auto v7 = v12.z + 1.7;

			auto v8 = pos->z;
			if( v8 > v7 ) // if (v8 > (v12 + 1.7))
			{
				float v13 = 0.f;
				float v3 = ( *pos ).z - v7;

				// changed this from float division to float multiplication cos its faster
				float v4 = ( v3 - 4.f ) * 0.16666667;
				if( v4 >= 0.f )
					v13 = std::fminf( v4, 1.f );

				( *pos ).z += ( ( v7 - ( *pos ).z ) * ( ( ( v13 * v13 ) * 3.f ) - ( ( ( v13 * v13 ) * 2.f ) * v13 ) ) );
			}
		}

		// ( *( *g_MdlCache + 136 ) )( g_MdlCache );

		// Idk what the fuck this does but it gets called on server so why not call it here
		// ((136 / 4) = 34) -> That's the server index, and on server they are offseted by one
		// Memory::GetVirtualFunction( Context::CSGO.ModelCache, 33 ).cast< void( __thiscall * )( void * )>( )( Context::CSGO.ModelCache );
	}
}

Vector C_CSPlayer::GetEyePosition( ) {
	// hey, modifying it by ourself isn't needed at all, this is way more accurate when landing etc.
	// hey alpha ur incorrect brother man. it is needed for accuracy.

	if( !this ) {
		return{ };
	}

	Vector pos;

	pos = C_BasePlayer::GetEyePosition( );

	if( *reinterpret_cast < int32_t * > ( uintptr_t( this ) + 0x39E1 ) ) {
		auto v3 = m_PlayerAnimState( );
		if( v3 ) {
			CCSGOPlayerAnimState::ModifyEyePosition( v3, &pos );
		}
	}

	return pos;
}