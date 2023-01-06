#include "Esp.hpp"
#include "../../SDK/Classes/entity.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/CVariables.hpp"
#include "../../source.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../Renderer/Render.hpp"
#include "../Rage/Autowall.h"
#include "Hitmarker.hpp"
#include "../../SDK/Classes/PropManager.hpp"
#include "../Rage/LagCompensation.hpp"
#include "EventLogger.hpp"
#include "../../Utils/InputSys.hpp"
#include "../Rage/Ragebot.hpp"
#include <minwindef.h>
#include "../../Utils/Math.h"
#include "../../SDK/RayTracer.h"
#include "../../SDK/displacement.hpp"
#include "CChams.hpp"
#include "../Rage/TickbaseShift.hpp"
#include "ExtendedEsp.hpp"
#include <sstream>
#include <ctime>
#include <windows.h>

//#include "../Rage/AnimationSystem.hpp"

#include "../Miscellaneous/Movement.hpp"

#include <iomanip>

extern Vector AutoPeekPos;

struct BBox_t {
	int x, y, w, h;
};

class CEsp : public IEsp {
public:
	void PenetrateCrosshair(Vector2D center);
	void DrawAntiAimIndicator() override;
	void DrawZeusDistance();
	void Main() override;
	void SetAlpha(int idx) override;
	float GetAlpha(int idx) override;
	void AddSkeletonMatrix(C_CSPlayer* player, matrix3x4_t* bones) override;
private:
	struct IndicatorsInfo_t {
		IndicatorsInfo_t() {

		}

		IndicatorsInfo_t(const char* m_szName,
			int m_iPrioirity,
			bool m_bLoading,
			float m_flLoading,
			FloatColor m_Color) {
			this->m_szName = m_szName;
			this->m_iPrioirity = m_iPrioirity;
			this->m_bLoading = m_bLoading;
			this->m_flLoading = m_flLoading;
			this->m_Color = m_Color;
		}

		const char* m_szName = "";
		int m_iPrioirity = -1;
		bool m_bLoading = false;
		float m_flLoading = 0.f;
		FloatColor m_Color = FloatColor(0, 0, 0, 255);
	};
	
	std::vector< IndicatorsInfo_t > m_vecTextIndicators;

	struct EspData_t {
		C_CSPlayer* player;
		bool bEnemy;
		Vector2D head_pos;
		Vector2D feet_pos;
		Vector origin;
		BBox_t bbox;
		player_info_t info;
	};

	EspData_t m_Data;
	C_CSPlayer* m_LocalPlayer = nullptr;
	C_CSPlayer* m_LocalObserved = nullptr;

	int storedTick = 0;
	int crouchedTicks[65];
	float m_flAlpha[65];

	float lastTime = 0.0f;
	int oldframecount = 0;
	int curfps = 0;

	bool m_bAlphaFix[65];
	bool Begin(C_CSPlayer* player);
	bool ValidPlayer(C_CSPlayer* player);
	void AmmoBar(C_CSPlayer* player, BBox_t bbox);
	void RenderNades(C_WeaponCSBaseGun* nade);
	void DrawBox(BBox_t bbox, const FloatColor& clr, C_CSPlayer* player);
	void DrawHealthBar(C_CSPlayer* player, BBox_t bbox);
	void DrawInfo(C_CSPlayer* player, BBox_t bbox, player_info_t player_info);
	void DrawBottomInfo(C_CSPlayer* player, BBox_t bbox, player_info_t player_info);
	void DrawName(C_CSPlayer* player, BBox_t bbox, player_info_t player_info);
	void DrawSkeleton(C_CSPlayer* player);
	void DrawHitSkeleton();
	bool GetBBox(C_BaseEntity* player, Vector2D screen_points[], BBox_t& outRect);
	void Offscreen();
	void OverlayInfo();
	void Indicators();
	void BloomEffect();
	bool IsFakeDucking(C_CSPlayer* player) {
		if (!player)
			return false;

		float duckamount = player->m_flDuckAmount();
		if (!duckamount) {
			crouchedTicks[player->EntIndex()] = 0;
			return false;
		}

		float duckspeed = player->m_flDuckSpeed();
		if (!duckspeed) {
			crouchedTicks[player->EntIndex()] = 0;
			return false;
		}

		if (storedTick != Interfaces::m_pGlobalVars->tickcount) {
			crouchedTicks[player->EntIndex()]++;
			storedTick = Interfaces::m_pGlobalVars->tickcount;
		}

		if (int(duckspeed) == 8 && duckamount <= 0.9f && duckamount > 0.01
			&& (player->m_fFlags() & FL_ONGROUND) && (crouchedTicks[player->EntIndex()] >= 5))
			return true;
		else
			return false;
	};

	C_Window m_KeyBinds = { Vector2D(g_Vars.esp.keybind_window_x, g_Vars.esp.keybind_window_y), Vector2D(180, 10), 0 };
	C_Window m_SpecList = { Vector2D(g_Vars.esp.spec_window_x, g_Vars.esp.spec_window_y), Vector2D(180, 10), 1 };

	void SpectatorList(bool window = false);
	void Keybinds();

	struct C_HitMatrixEntry {
		float m_flTime = 0.0f;
		float m_flAlpha = 0.0f;

		C_CSPlayer* m_pEntity = nullptr;
		matrix3x4_t pBoneToWorld[128] = { };
	};

	std::vector<C_HitMatrixEntry> m_Hitmatrix;
};

void CEsp::AddSkeletonMatrix(C_CSPlayer* player, matrix3x4_t* bones) {
	if (!player || !bones)
		return;

	C_HitMatrixEntry info;

	info.m_flTime = Interfaces::m_pGlobalVars->realtime + g_Vars.esp.hitskeleton_time;
	info.m_flAlpha = 1.f;
	info.m_pEntity = player;
	std::memcpy(info.pBoneToWorld, bones, player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));

	m_Hitmatrix.push_back(info);
}

void CEsp::BloomEffect() {
	static bool props = false;

	static ConVar* r_modelAmbientMin = Interfaces::m_pCvar->FindVar(XorStr("r_modelAmbientMin"));

	for (int i = 0; i < Interfaces::m_pEntList->GetHighestEntityIndex(); i++) {
		C_BaseEntity* pEntity = (C_BaseEntity*)Interfaces::m_pEntList->GetClientEntity(i);

		if (!pEntity)
			continue;

		if (pEntity->GetClientClass()->m_ClassID == 69) {
			auto pToneMap = (CEnvTonemapContorller*)pEntity;
			if (pToneMap) {
				*pToneMap->m_bUseCustomAutoExposureMin() = true;
				*pToneMap->m_bUseCustomAutoExposureMax() = true;

				*pToneMap->m_flCustomAutoExposureMin() = 0.2f;
				*pToneMap->m_flCustomAutoExposureMax() = 0.2f;
				*pToneMap->m_flCustomBloomScale() = 10.1f;

				r_modelAmbientMin->SetValue(g_Vars.esp.model_brightness);
			}
		}

		if (pEntity->GetClientClass()->m_ClassID == CPrecipitation) {
			auto pToneMap = pEntity;
			if (pToneMap) {

			}
		}
	}
}

void DrawWatermark() {

	if (!g_Vars.misc.watermark)
		return;

	bool connected = Interfaces::m_pEngine->IsInGame();
	std::string text;

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	
	// ez convert to 12 hour format
	if (lt.wHour > 12)
		lt.wHour -= 12;
	if (lt.wHour == 0)
		lt.wHour = 12;

	std::string hour = std::to_string(lt.wHour);
	std::string wMin = std::to_string(lt.wMinute);
	std::string wSec = std::to_string(lt.wSecond);
	
	// add 0 padding to minutes and seconds
	size_t n_zeros = 2;
	auto min = std::string(n_zeros - std::min(n_zeros, wMin.length()), '0') + wMin;
	auto sec = std::string(n_zeros - std::min(n_zeros, wSec.length()), '0') + wSec;

	player_info_t info;
	Interfaces::m_pEngine->GetPlayerInfo(Interfaces::m_pEngine->GetLocalPlayer(), &info);

	if (connected) {
		auto netchannel = Encrypted_t<INetChannelInfo>(Interfaces::m_pEngine->GetNetChannelInfo());
		if (!netchannel.IsValid())
			return;
		int ms = (std::max(0, (int)std::round(netchannel->GetLatency(FLOW_OUTGOING) * 1000.f)) + std::max(0, (int)std::round(netchannel->GetLatency(FLOW_INCOMING) * 1000.f)));
		
		text = XorStr("Autism | ");
		text += info.szName;
		text += XorStr(" | ");
		text += XorStr(g_Vars.globals.server_adress);
		text += XorStr(" | ");
		text += std::to_string(ms);
		text += XorStr(" ms | ");
		text += hour; text += XorStr(":"); text += min; text += XorStr(":"); text += sec;
	}
	else {
		text = XorStr("Autism | ");
		text += hour; text += XorStr(":"); text += min; text += XorStr(":"); text += sec;
	}

	Render::Engine::FontSize_t size = Render::Engine::watermark.size(text);

	Vector2D screen = Render::GetScreenSize();

	// background.
	Render::Engine::RectFilled(screen.x - size.m_width - 20, 10, size.m_width + 10, size.m_height + 4, Color(0, 0, 0, 210));
	Render::Engine::RectFilled(screen.x - size.m_width - 20, 10, size.m_width + 10, size.m_height - 11, g_Vars.menu.ascent.ToRegularColor());

	// text.
	Render::Engine::watermark.string(screen.x - 15, 12, { 255, 255, 255, 220 }, text, Render::Engine::ALIGN_RIGHT);
}

bool CEsp::Begin(C_CSPlayer* player) {
	m_Data.player = player;
	m_Data.bEnemy = player->m_iTeamNum() != m_LocalPlayer->m_iTeamNum();
	m_LocalObserved = (C_CSPlayer*)m_LocalPlayer->m_hObserverTarget().Get();

	player_info_t player_info;
	if (!Interfaces::m_pEngine->GetPlayerInfo(player->EntIndex(), &player_info))
		return false;

	m_Data.info = player_info;

	if (!m_Data.bEnemy)
		return false;

	Vector2D points[8];
	return GetBBox(player, points, m_Data.bbox);
}

bool CEsp::ValidPlayer(C_CSPlayer* player) {
	if (!player)
		return false;

	int idx = player->EntIndex();

	if (player->IsDead()) {
		m_flAlpha[idx] = 0.f;
		return false;
	}

	static auto g_GameRules = *(uintptr_t**)(Engine::Displacement.Data.m_GameRules);
	if (*(bool*)(*(uintptr_t*)g_GameRules + 0x20)) {
		if (player->IsDormant()) {
			m_flAlpha[idx] = 0.f;
		}
		return false;
	}
	if (player->IsDormant()) {
		if (m_flAlpha[idx] < 0.6f) {
			m_flAlpha[idx] -= (1.0f / 1.0f) * Interfaces::m_pGlobalVars->frametime;
			m_flAlpha[idx] = std::clamp(m_flAlpha[idx], 0.f, 0.6f);
		}
		else {
			m_flAlpha[idx] -= (1.0f / 20.f) * Interfaces::m_pGlobalVars->frametime;
		}
	}
	else {
		m_flAlpha[idx] += (1.0f / 0.2f) * Interfaces::m_pGlobalVars->frametime;
		m_flAlpha[idx] = std::clamp(m_flAlpha[idx], 0.f, 1.f);
	}

	return (m_flAlpha[idx] > 0.f);
}

int fps() {
	static float m_Framerate = 0.f;

	// Move rolling average
	m_Framerate = 0.9 * m_Framerate + (1.0 - 0.9) * Interfaces::m_pGlobalVars->absoluteframetime;

	if (m_Framerate <= 0.0f)
		m_Framerate = 1.0f;

	return (int)(1.0f / m_Framerate);
}

void CEsp::Indicators() {

	if (auto pLocal = C_CSPlayer::GetLocalPlayer(); pLocal) {

		struct Indicator_t { Color color; std::string text; };
		std::vector< Indicator_t > indicators{ };

		Color ff15c27b{ 124, 195, 13, 255 };
		Color ff0000ff{ 255,0,0,255 };

		if (g_Vars.antiaim.draw_angles)
		{
			const auto& pos = pLocal->GetAbsOrigin();
			Vector2D tmp;

			if (WorldToScreen(pos, tmp))
			{
				Vector2D draw_tmp;
				const Vector real_pos(45.f * std::cos(DEG2RAD(g_Vars.globals.FakeAngle.y)) + pos.x, 45.f * sin(DEG2RAD(g_Vars.globals.FakeAngle.y)) + pos.y, pos.z);

				if (WorldToScreen(real_pos, draw_tmp))
				{
					Render::Engine::Line(tmp.x, tmp.y, draw_tmp.x, draw_tmp.y, { 0, 255, 0, 255 });
					Render::Engine::esp.string(draw_tmp.x, draw_tmp.y, { 0, 255, 0, 255 }, "FAKE", Render::Engine::ALIGN_LEFT);
				}

				const Vector fake_pos(45.f * cos(DEG2RAD(g_Vars.globals.RegularAngles.y)) + pos.x, 45.f * sin(DEG2RAD(g_Vars.globals.RegularAngles.y)) + pos.y, pos.z);

				if (WorldToScreen(fake_pos, draw_tmp))
				{
					Render::Engine::Line(tmp.x, tmp.y, draw_tmp.x, draw_tmp.y, { 255, 0, 0, 255 });
					Render::Engine::esp.string(draw_tmp.x, draw_tmp.y, { 255, 0, 0, 255 }, "REAL", Render::Engine::ALIGN_LEFT);
				}

				float lby = pLocal->m_flLowerBodyYawTarget();
				const Vector lby_pos(45.f * cos(DEG2RAD(lby)) + pos.x, 45.f * sin(DEG2RAD(lby)) + pos.y, pos.z);
				if (WorldToScreen(lby_pos, draw_tmp))
				{
					Render::Engine::Line(tmp.x, tmp.y, draw_tmp.x, draw_tmp.y, { 255, 255, 255, 255 });
					Render::Engine::esp.string(draw_tmp.x, draw_tmp.y, { 255, 255, 255, 255 }, "LBY", Render::Engine::ALIGN_LEFT);
				}
			}
		}

		// LC
		if (!(pLocal->m_fFlags() & FL_ONGROUND) && g_Vars.esp.indicator_breaklc) {
			Indicator_t ind{ };
			ind.color = g_Vars.globals.bBrokeLC ? ff15c27b : ff0000ff;
			ind.text = ("LC");

			indicators.push_back(ind);
		}

		// get the absolute change between current lby and animated angle.
		float change = std::abs(Math::AngleNormalize(g_Vars.globals.m_flBody - g_Vars.globals.RegularAngles.y));

		if (g_Vars.esp.indicator_lby) {
			Indicator_t ind{ };
			ind.color = change > 35.f ? ff15c27b : ff0000ff;
			ind.text = ("LBY");
			indicators.push_back(ind);
		}

		if (g_Vars.rage.key_dmg_override.enabled && g_Vars.globals.OverridingMinDmg && g_Vars.esp.indicator_dmg) {
			Indicator_t ind{ };
			ind.color = Color(201, 201, 201, 255);
			ind.text = ("DMG");
			indicators.push_back(ind);
		}

		if (indicators.empty())
			return;

		// iterate and draw indicators.
		for (size_t i{ }; i < indicators.size(); ++i) {
			auto& indicator = indicators[i];

			Render::Engine::indi.string(12, Render::GetScreenSize().y - 64 - (30 * i), indicator.color, indicator.text);
		}

	}
}

void CEsp::SpectatorList(bool window) {

	std::vector< std::string > spectators{ "" };
	int h = Render::Engine::esp.m_size.m_height;
	C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer();

	for (int i{ 1 }; i <= Interfaces::m_pGlobalVars->maxClients; ++i) {
		C_CSPlayer* player = (C_CSPlayer*)Interfaces::m_pEntList->GetClientEntity(i);
		if (!player)
			continue;

		if (player == pLocal)
			continue;

		if (player->IsDormant())
			continue;

		if (player->m_iHealth() > 0)
			continue;

		if (player->m_hObserverTarget() != pLocal)
			continue;

		player_info_t info;
		if (!Interfaces::m_pEngine->GetPlayerInfo(i, &info))
			continue;

		auto specplayer = player->m_hObserverTarget() == pLocal;
		if (!specplayer) 
			continue;

		spectators.push_back(std::string(info.szName).substr(0, 24));
	}

	size_t total_size = spectators.size() * (h - 1);

	for (size_t i{ }; i < spectators.size(); ++i) {
		const std::string& name = spectators[i];

		Render::Engine::esp.string(Render::GetScreenSize().x - 5, (i * 14) - 10, { 255, 255, 255, 179 }, name, Render::Engine::ALIGN_RIGHT);
	}
}

void CEsp::Keybinds() {
	std::vector<
		std::pair<std::string, int>
	> vecNames;

	this->m_KeyBinds.Drag();

	Vector2D pos = { g_Vars.esp.keybind_window_x, g_Vars.esp.keybind_window_y };

	this->m_KeyBinds.size.y = 20.0f;

	auto AddBind = [this, &vecNames](const char* name, KeyBind_t& bind) {
		if (!bind.enabled)
			return;

		if (!vecNames.empty())
			this->m_KeyBinds.size.y += 13.0f;

		vecNames.push_back(std::pair<std::string, int>(std::string(name), bind.cond));
	};

	if (g_Vars.rage.enabled) {
		AddBind(XorStr("Damage override"), g_Vars.rage.key_dmg_override);
		AddBind(XorStr("Hitbox override"), g_Vars.rage.hitbox_override_bind);
	}

	if (g_Vars.misc.edgejump) {
		AddBind(XorStr("Edge jump"), g_Vars.misc.edgejump_bind);
	}

	if (g_Vars.misc.autopeek) {
		AddBind(XorStr("Auto peek"), g_Vars.misc.autopeek_bind);
	}

	if (g_Vars.misc.slow_walk) {
		AddBind(XorStr("Fake walk"), g_Vars.misc.slow_walk_bind);
	}

	if (g_Vars.misc.move_exploit) {
		AddBind(XorStr("Exploit walk"), g_Vars.misc.move_exploit_key);
	}

	float gaySize = this->m_KeyBinds.size.y;

	static float alpha = 0.f;
	bool condition = ((vecNames.empty() && g_Vars.globals.menuOpen) || !vecNames.empty());
	float multiplier = static_cast<float>((1.0f / 0.05f) * Interfaces::m_pGlobalVars->frametime);
	if (condition) {
		alpha += multiplier * (1.0f - alpha);
	}
	else {
		if (alpha > 0.01f)
			alpha += multiplier * (0.0f - alpha);
		else
			alpha = 0.0f;
	}

	alpha = std::clamp(alpha, 0.f, 1.0f);

	if (alpha <= 0.f)
		return;

	Color main = Color(39, 41, 54, 150 * alpha);
	Color accent = g_Vars.menu.ascent.ToRegularColor();
	accent.RGBA[3] *= alpha;

	this->m_KeyBinds.size.y = 20.0f;

	// header
	Render::Engine::RectFilled(pos, this->m_KeyBinds.size, Color(39, 41, 54, 220 * alpha));

	// line splitting
	Render::Engine::Line(pos + Vector2D(0, this->m_KeyBinds.size.y), pos + this->m_KeyBinds.size, accent);
	Render::Engine::Line(pos + Vector2D(0, this->m_KeyBinds.size.y + 1), pos + Vector2D(this->m_KeyBinds.size.x, this->m_KeyBinds.size.y + 1), accent);

	this->m_KeyBinds.size.y = gaySize;

	// the actual window
	Render::Engine::RectFilled(pos + Vector2D(0, 20 + 2), Vector2D(this->m_KeyBinds.size.x, this->m_KeyBinds.size.y - 1), main);

	auto hold_size = Render::Engine::esp.size(XorStr("[holding]"));
	auto toggle_size = Render::Engine::esp.size(XorStr("[toggled]"));
	auto always_size = Render::Engine::esp.size(XorStr("[always]"));

	if (!vecNames.empty()) {
		float offset = 14.0f;
		for (auto name : vecNames) {
			// hotkey name
			Render::Engine::esp.string(pos.x + 5, pos.y + 9 + offset, Color::White().OverrideAlpha(255 * alpha), name.first.c_str(), Render::Engine::ALIGN_LEFT);

			// hotkey type
			Render::Engine::esp.string(pos.x + this->m_KeyBinds.size.x - 5, pos.y + 9 + offset, Color::White().OverrideAlpha(255 * alpha),
				name.second == KeyBindType::HOLD ? XorStr("[holding]") : name.second == KeyBindType::TOGGLE ? XorStr("[toggled]") : XorStr("[always]"), Render::Engine::ALIGN_RIGHT);

			// add offset
			offset += 14.0f;
		}
	}

	// title
	auto size = Render::Engine::esp.size(XorStr("Keybinds"));
	Render::Engine::esp.string(pos.x + (this->m_KeyBinds.size.x * 0.5) - 2, pos.y + (size.m_height * 0.5) - 4, Color::White().OverrideAlpha(255 * alpha), XorStr("Keybinds"), Render::Engine::ALIGN_CENTER);
}

// lol
bool IsAimingAtPlayerThroughPenetrableWall(C_CSPlayer* local, C_WeaponCSBaseGun* pWeapon) {
	auto weaponInfo = pWeapon->GetCSWeaponData();
	if (!weaponInfo.IsValid())
		return -1.0f;

	QAngle view_angles;
	Interfaces::m_pEngine->GetViewAngles(view_angles);

	Autowall::C_FireBulletData data;

	data.m_Player = local;
	data.m_TargetPlayer = nullptr;
	data.m_bPenetration = true;
	data.m_vecStart = local->GetEyePosition();
	data.m_vecDirection = view_angles.ToVectors();
	data.m_flMaxLength = data.m_vecDirection.Normalize();
	data.m_WeaponData = weaponInfo.Xor();
	data.m_flCurrentDamage = static_cast<float>(weaponInfo->m_iWeaponDamage);

	return Autowall::FireBullets(&data) >= 1.f;
}

float GetPenetrationDamage(C_CSPlayer* local, C_WeaponCSBaseGun* pWeapon) {
	auto weaponInfo = pWeapon->GetCSWeaponData();
	if (!weaponInfo.IsValid())
		return -1.0f;

	Autowall::C_FireBulletData data;

	data.m_iPenetrationCount = 4;
	data.m_Player = local;
	data.m_TargetPlayer = nullptr;

	QAngle view_angles;
	Interfaces::m_pEngine->GetViewAngles(view_angles);
	data.m_vecStart = local->GetEyePosition();
	data.m_vecDirection = view_angles.ToVectors();
	data.m_flMaxLength = data.m_vecDirection.Normalize();
	data.m_WeaponData = weaponInfo.Xor();
	data.m_flTraceLength = 0.0f;

	data.m_flCurrentDamage = static_cast<float>(weaponInfo->m_iWeaponDamage);

	CTraceFilter filter;
	filter.pSkip = local;

	Vector end = data.m_vecStart + data.m_vecDirection * weaponInfo->m_flWeaponRange;

	Autowall::TraceLine(data.m_vecStart, end, MASK_SHOT_HULL | CONTENTS_HITBOX, &filter, &data.m_EnterTrace);
	Autowall::ClipTraceToPlayers(data.m_vecStart, end + data.m_vecDirection * 40.0f, MASK_SHOT_HULL | CONTENTS_HITBOX, &filter, &data.m_EnterTrace);
	if (data.m_EnterTrace.fraction == 1.f)
		return -1.0f;

	data.m_flTraceLength += data.m_flMaxLength * data.m_EnterTrace.fraction;
	if (data.m_flMaxLength != 0.0f && data.m_flTraceLength >= data.m_flMaxLength)
		return data.m_flCurrentDamage;

	data.m_flCurrentDamage *= powf(weaponInfo->m_flRangeModifier, data.m_flTraceLength * 0.002f);
	data.m_EnterSurfaceData = Interfaces::m_pPhysSurface->GetSurfaceData(data.m_EnterTrace.surface.surfaceProps);

	C_BasePlayer* hit_player = static_cast<C_BasePlayer*>(data.m_EnterTrace.hit_entity);
	bool can_do_damage = (data.m_EnterTrace.hitgroup >= Hitgroup_Head && data.m_EnterTrace.hitgroup <= Hitgroup_Gear);
	bool hit_target = !data.m_TargetPlayer || hit_player == data.m_TargetPlayer;
	if (can_do_damage && hit_player && hit_player->EntIndex() <= Interfaces::m_pGlobalVars->maxClients && hit_player->EntIndex() > 0 && hit_target) {
		if (pWeapon && pWeapon->m_iItemDefinitionIndex() == WEAPON_ZEUS)
			return (data.m_flCurrentDamage * 0.9f);

		if (pWeapon->m_iItemDefinitionIndex() == WEAPON_ZEUS) {
			data.m_EnterTrace.hitgroup = Hitgroup_Generic;
		}

		data.m_flCurrentDamage = Autowall::ScaleDamage((C_CSPlayer*)hit_player, data.m_flCurrentDamage, weaponInfo->m_flArmorRatio, data.m_EnterTrace.hitgroup);
		return data.m_flCurrentDamage;
	};

	if (data.m_flTraceLength > 3000.0f && weaponInfo->m_flPenetration > 0.f || 0.1f > data.m_EnterSurfaceData->game.flPenetrationModifier)
		return -1.0f;

	if (Autowall::HandleBulletPenetration(&data))
		return -1.0f;

	return data.m_flCurrentDamage;
};

void CEsp::PenetrateCrosshair(Vector2D center) {
	C_CSPlayer* local = C_CSPlayer::GetLocalPlayer();
	if (!local || local->IsDead())
		return;

	C_WeaponCSBaseGun* pWeapon = (C_WeaponCSBaseGun*)local->m_hActiveWeapon().Get();
	if (!pWeapon)
		return;

	if (!pWeapon->GetCSWeaponData().IsValid())
		return;

	auto type = pWeapon->GetCSWeaponData().Xor()->m_iWeaponType;

	if (type == WEAPONTYPE_KNIFE || type == WEAPONTYPE_C4 || type == WEAPONTYPE_GRENADE)
		return;

	// fps enhancer
	g_Vars.globals.m_nPenetrationDmg = (int)GetPenetrationDamage(local, pWeapon);
	g_Vars.globals.m_bAimAtEnemyThruWallOrVisibleLoool = IsAimingAtPlayerThroughPenetrableWall(local, pWeapon);
	Color color = g_Vars.globals.m_bAimAtEnemyThruWallOrVisibleLoool ? (Color(0, 30, 225, 210)) : (g_Vars.globals.m_nPenetrationDmg >= 1 ? Color(0, 255, 0, 210) : Color(255, 0, 0, 210));

	if (g_Vars.esp.autowall_crosshair) {
		Render::Engine::RectFilled(center - 1, { 3, 3 }, Color(0, 0, 0, 125));
		Render::Engine::RectFilled(Vector2D(center.x, center.y - 1), Vector2D(1, 3), color);
		Render::Engine::RectFilled(Vector2D(center.x - 1, center.y), Vector2D(3, 1), color);
	}
}

void CEsp::DrawAntiAimIndicator() {
	if (!g_Vars.antiaim.manual || !Interfaces::m_pEngine->IsInGame())
		return;

	C_CSPlayer* local = C_CSPlayer::GetLocalPlayer();
	if (!local || local->IsDead())
		return;

	//if( g_Vars.globals.manual_aa == -1 )
	//	return;

	bool bLeft = false, bRight = false, bBack = false;
	switch (g_Vars.globals.manual_aa) {
	case 0:
		bLeft = true;
		bRight = false;
		bBack = false;
		break;
	case 1:
		bLeft = false;
		bRight = false;
		bBack = true;
		break;
	case 2:
		bLeft = false;
		bRight = true;
		bBack = false;
		break;
	}

	Color color = g_Vars.antiaim.manual_color.ToRegularColor();

	float alpha = floor(sin(Interfaces::m_pGlobalVars->realtime * 4) * (color.RGBA[3] / 2 - 1) + color.RGBA[3] / 2);

	color.RGBA[3] = alpha;

	// Polygon points aka arrows
	auto ScreenSize = Render::GetScreenSize();
	int x = ScreenSize.x / 2;
	int y = ScreenSize.y / 2;

	if (bLeft) {
		Render::Engine::RectFilled(x - 50, y - 10, 1, 21, color);
		Render::Engine::RectFilled(x - 51, y - 10 + 1, 1, 19, color);
		Render::Engine::RectFilled(x - 52, y - 10 + 2, 1, 17, color);
		Render::Engine::RectFilled(x - 53, y - 10 + 3, 1, 15, color);
		Render::Engine::RectFilled(x - 54, y - 10 + 4, 1, 13, color);
		Render::Engine::RectFilled(x - 55, y - 10 + 5, 1, 11, color);
		Render::Engine::RectFilled(x - 56, y - 10 + 6, 1, 9, color);
		Render::Engine::RectFilled(x - 57, y - 10 + 7, 1, 7, color);
		Render::Engine::RectFilled(x - 58, y - 10 + 8, 1, 5, color);
		Render::Engine::RectFilled(x - 59, y - 10 + 9, 1, 3, color);
		Render::Engine::RectFilled(x - 60, y - 10 + 10, 1, 1, color);
	}
	else {
		Render::Engine::RectFilled(x - 50, y - 10, 1, 21, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 51, y - 10 + 1, 1, 19, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 52, y - 10 + 2, 1, 17, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 53, y - 10 + 3, 1, 15, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 54, y - 10 + 4, 1, 13, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 55, y - 10 + 5, 1, 11, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 56, y - 10 + 6, 1, 9, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 57, y - 10 + 7, 1, 7, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 58, y - 10 + 8, 1, 5, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 59, y - 10 + 9, 1, 3, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 60, y - 10 + 10, 1, 1, { 0, 0, 0, 125 });
	}

	if (bRight) {
		Render::Engine::RectFilled(x + 50, y - 10, 1, 21, color);
		Render::Engine::RectFilled(x + 51, y - 10 + 1, 1, 19, color);
		Render::Engine::RectFilled(x + 52, y - 10 + 2, 1, 17, color);
		Render::Engine::RectFilled(x + 53, y - 10 + 3, 1, 15, color);
		Render::Engine::RectFilled(x + 54, y - 10 + 4, 1, 13, color);
		Render::Engine::RectFilled(x + 55, y - 10 + 5, 1, 11, color);
		Render::Engine::RectFilled(x + 56, y - 10 + 6, 1, 9, color);
		Render::Engine::RectFilled(x + 57, y - 10 + 7, 1, 7, color);
		Render::Engine::RectFilled(x + 58, y - 10 + 8, 1, 5, color);
		Render::Engine::RectFilled(x + 59, y - 10 + 9, 1, 3, color);
		Render::Engine::RectFilled(x + 60, y - 10 + 10, 1, 1, color);
	}
	else {
		Render::Engine::RectFilled(x + 50, y - 10, 1, 21, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 51, y - 10 + 1, 1, 19, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 52, y - 10 + 2, 1, 17, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 53, y - 10 + 3, 1, 15, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 54, y - 10 + 4, 1, 13, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 55, y - 10 + 5, 1, 11, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 56, y - 10 + 6, 1, 9, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 57, y - 10 + 7, 1, 7, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 58, y - 10 + 8, 1, 5, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 59, y - 10 + 9, 1, 3, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x + 60, y - 10 + 10, 1, 1, { 0, 0, 0, 125 });
	}
	if (bBack) {
		Render::Engine::RectFilled(x - 10, y + 50, 21, 1, color);
		Render::Engine::RectFilled(x - 10 + 1, y + 51, 19, 1, color);
		Render::Engine::RectFilled(x - 10 + 2, y + 52, 17, 1, color);
		Render::Engine::RectFilled(x - 10 + 3, y + 53, 15, 1, color);
		Render::Engine::RectFilled(x - 10 + 4, y + 54, 13, 1, color);
		Render::Engine::RectFilled(x - 10 + 5, y + 55, 11, 1, color);
		Render::Engine::RectFilled(x - 10 + 6, y + 56, 9, 1, color);
		Render::Engine::RectFilled(x - 10 + 7, y + 57, 7, 1, color);
		Render::Engine::RectFilled(x - 10 + 8, y + 58, 5, 1, color);
		Render::Engine::RectFilled(x - 10 + 9, y + 59, 3, 1, color);
		Render::Engine::RectFilled(x - 10 + 10, y + 60, 1, 1, color);
	}
	else {
		Render::Engine::RectFilled(x - 10, y + 50, 21, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 1, y + 51, 19, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 2, y + 52, 17, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 3, y + 53, 15, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 4, y + 54, 13, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 5, y + 55, 11, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 6, y + 56, 9, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 7, y + 57, 7, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 8, y + 58, 5, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 9, y + 59, 3, 1, { 0, 0, 0, 125 });
		Render::Engine::RectFilled(x - 10 + 10, y + 60, 1, 1, { 0, 0, 0, 125 });
	}
}

void CEsp::DrawZeusDistance() {
	if (!g_Vars.esp.zeus_distance)
		return;

	C_CSPlayer* pLocalPlayer = C_CSPlayer::GetLocalPlayer();

	if (!pLocalPlayer || pLocalPlayer->IsDead())
		return;

	C_WeaponCSBaseGun* pWeapon = (C_WeaponCSBaseGun*)pLocalPlayer->m_hActiveWeapon().Get();

	if (!pWeapon)
		return;

	auto pWeaponInfo = pWeapon->GetCSWeaponData();
	if (!pWeaponInfo.IsValid())
		return;

	if (!(pWeapon->m_iItemDefinitionIndex() == WEAPON_ZEUS))
		return;

	auto collision = pLocalPlayer->m_Collision();
	Vector eyePos = pLocalPlayer->GetAbsOrigin() + (collision->m_vecMins + collision->m_vecMaxs) * 0.5f;

	float flBestDistance = FLT_MAX;
	C_CSPlayer* pBestPlayer = nullptr;

	for (int i = 1; i <= Interfaces::m_pGlobalVars->maxClients; i++) {

		C_CSPlayer* player = (C_CSPlayer*)(Interfaces::m_pEntList->GetClientEntity(i));

		if (!player || player->IsDead() || player->IsDormant())
			continue;

		float Dist = pLocalPlayer->m_vecOrigin().Distance(player->m_vecOrigin());

		if (Dist < flBestDistance) {
			flBestDistance = Dist;
			pBestPlayer = player;
		}
	}

	auto GetZeusRange = [&](C_CSPlayer* player) -> float {
		const float RangeModifier = 0.00490000006f, MaxDamage = 500.f;
		return (log(player->m_iHealth() / MaxDamage) / log(RangeModifier)) / 0.002f;
	};

	float flRange = 0.f;
	if (pBestPlayer) {
		flRange = GetZeusRange(pBestPlayer);
	}

	const int accuracy = 360;
	const float step = DirectX::XM_2PI / accuracy;
	for (float a = 0.0f; a < DirectX::XM_2PI; a += step) {
		float a_c, a_s, as_c, as_s;
		DirectX::XMScalarSinCos(&a_s, &a_c, a);
		DirectX::XMScalarSinCos(&as_s, &as_c, a + step);

		Vector startPos = Vector(a_c * flRange + eyePos.x, a_s * flRange + eyePos.y, eyePos.z);
		Vector endPos = Vector(as_c * flRange + eyePos.x, as_s * flRange + eyePos.y, eyePos.z);

		Ray_t ray;
		CGameTrace tr;
		CTraceFilter filter = CTraceFilter();
		filter.pSkip = pLocalPlayer;

		ray.Init(eyePos, startPos);
		Interfaces::m_pEngineTrace->TraceRay(ray, MASK_SOLID, &filter, &tr);

		auto frac_1 = tr.fraction;
		Vector2D start2d;
		if (!WorldToScreen(tr.endpos, start2d))
			continue;

		ray.Init(eyePos, endPos);
		Interfaces::m_pEngineTrace->TraceRay(ray, MASK_SOLID, &filter, &tr);

		Vector2D end2d;

		if (!WorldToScreen(tr.endpos, end2d))
			continue;

		Render::Engine::Line(start2d, end2d, g_Vars.esp.zeus_distance_color.ToRegularColor());
	}
}

void CEsp::Main() {
	DrawWatermark();

	if (g_Vars.esp.keybind_window_enabled)
		Keybinds();

	if (g_Vars.esp.spec_window_enabled)
		SpectatorList(true);

	m_LocalPlayer = C_CSPlayer::GetLocalPlayer();
	if (!g_Vars.globals.HackIsReady || !m_LocalPlayer || !Interfaces::m_pEngine->IsInGame())
		return;

	if (g_Vars.esp.remove_scope && g_Vars.esp.remove_scope_type == 0 && (m_LocalPlayer && m_LocalPlayer->m_hActiveWeapon().Get() && ((C_WeaponCSBaseGun*)m_LocalPlayer->m_hActiveWeapon().Get())->GetCSWeaponData().IsValid() && ((C_WeaponCSBaseGun*)m_LocalPlayer->m_hActiveWeapon().Get())->GetCSWeaponData()->m_iWeaponType == WEAPONTYPE_SNIPER_RIFLE && m_LocalPlayer->m_bIsScoped())) {
		Interfaces::m_pSurface->DrawSetColor(Color(0, 0, 0, 255));
		Vector2D center = Render::GetScreenSize();
		Interfaces::m_pSurface->DrawLine(center.x / 2, 0, center.x / 2, center.y);
		Interfaces::m_pSurface->DrawLine(0, center.y / 2, center.x, center.y / 2);

	}

	if (!m_LocalPlayer->IsDead())
		Indicators();

	// draw the damage at the latest pos we hit an enemy at
	if (g_Vars.esp.visualize_damage && Hitmarkers::m_vecWorldHitmarkers.size()) {
		Hitmarkers::Hitmarkers_t& info = Hitmarkers::m_vecWorldHitmarkers.back();
		Vector2D vecPos;
		if (WorldToScreen(Vector(info.m_flPosX, info.m_flPosY, info.m_flPosZ), vecPos)) {
			auto vecTextSize = Render::Engine::esp.size(std::to_string(Hitmarkers::m_nLastDamageData.second));

			Vector2D vecRenderPos = vecPos - Vector2D(vecTextSize.m_width / 2, 8 + vecTextSize.m_height);
			Render::Engine::esp.string(vecRenderPos.x, vecRenderPos.y, Hitmarkers::m_nLastDamageData.first.OverrideAlpha(220 * info.m_flAlpha),
				std::to_string(Hitmarkers::m_nLastDamageData.second));
		}
	}

	if (!g_Vars.esp.esp_enable)
		return;

	OverlayInfo();

	static float auto_peek_radius = 0.f;
	bool condition = g_Vars.misc.autopeek && g_Vars.misc.autopeek_visualise && !AutoPeekPos.IsZero() && g_Vars.misc.autopeek_bind.enabled;
	float multiplier = static_cast<float>((1.0f / 0.05f) * Interfaces::m_pGlobalVars->frametime);
	if (condition) {
		auto_peek_radius += multiplier * (1.0f - auto_peek_radius);
	}
	else {
		// makes the animation end faster
		// if we dont do dis ther is like a 1.f radius circle for like 1 second
		if (auto_peek_radius > 0.01f)
			auto_peek_radius += multiplier * (0.0f - auto_peek_radius);
		else
			auto_peek_radius = 0.0f;
	}

	auto_peek_radius = std::clamp(auto_peek_radius, 0.f, 1.0f);

	// fixes the fadeout disappearing rlly fast
	static Vector last_autopeek_pos = AutoPeekPos;
	if (!AutoPeekPos.IsZero()) {
		last_autopeek_pos = AutoPeekPos;
	}

	if (auto_peek_radius > 0.f) {
		//* 0.4f
		Render::Engine::WorldCircle(AutoPeekPos.IsZero() ? last_autopeek_pos : AutoPeekPos, 15.f * auto_peek_radius,
			g_Vars.misc.autopeek_color.ToRegularColor(), g_Vars.misc.autopeek_color.ToRegularColor().OverrideAlpha(g_Vars.misc.autopeek_color.ToRegularColor().a() * 0.4f, true));
	}

	if (!g_Vars.globals.vecExploitOrigin.IsZero() && g_Vars.globals.bMoveExploiting) {
		g_Vars.globals.vecExploitOrigin.z = m_LocalPlayer->GetAbsOrigin().z;
		Render::Engine::WorldCircle(g_Vars.globals.vecExploitOrigin, 5.f, Color(0, 255, 0, 100), Color(0, 255, 0, 50));

		Vector2D origin;
		Vector2D origin2;
		if (WorldToScreen(m_LocalPlayer->GetAbsOrigin(), origin)) {
			if (WorldToScreen(g_Vars.globals.vecExploitOrigin, origin2)) {
				Render::Engine::Line(origin, origin2, Color(255, 255, 255, 255));
			}
		}
	}

	DrawZeusDistance();

	Vector2D points[8];
	Vector2D center;


	//if( g_Vars.esp.hitskeleton )
	//	DrawHitSkeleton( );

	for (int i = 0; i <= Interfaces::m_pEntList->GetHighestEntityIndex(); ++i) {
		auto entity = (C_BaseEntity*)Interfaces::m_pEntList->GetClientEntity(i);

		if (!entity)
			continue;

		if (!entity->GetClientClass() /*|| !entity->GetClientClass( )->m_ClassID*/)
			continue;

		if (g_Vars.esp.nades) {
			if (entity->GetClientClass()->m_ClassID == CInferno) {
				C_Inferno* pInferno = reinterpret_cast<C_Inferno*>(entity);
				C_CSPlayer* player = (C_CSPlayer*)entity->m_hOwnerEntity().Get();

				if (player) {
					FloatColor color = FloatColor(1.f, 0.f, 0.f, 0.8f);

					if (player->m_iTeamNum() == m_LocalPlayer->m_iTeamNum() && player->EntIndex() != m_LocalPlayer->EntIndex()) {
						if (g_Vars.mp_friendlyfire && g_Vars.mp_friendlyfire->GetInt() == 0) {
							color = FloatColor(0, 0, 0, 0);
						}
					}

					const Vector origin = pInferno->GetAbsOrigin();
					Vector2D screen_origin = Vector2D();

					if (WorldToScreen(origin, screen_origin)) {
						struct s {
							Vector2D a, b, c;
						};
						std::vector<int> excluded_ents;
						std::vector<s> valid_molotovs;

						const auto spawn_time = pInferno->m_flSpawnTime();
						const auto time = ((spawn_time + C_Inferno::GetExpiryTime()) - Interfaces::m_pGlobalVars->curtime);

						if (time > 0.05f) {
							static const auto size = Vector2D(70.f, 4.f);

							auto new_pos = Vector2D(screen_origin.x - size.x * 0.5, screen_origin.y - size.y * 0.5);

							Vector min, max;
							entity->GetClientRenderable()->GetRenderBounds(min, max);

							auto radius = (max - min).Length2D() * 0.5f;
							Vector boundOrigin = Vector((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, min.z + 5) + origin;
							const int accuracy = 25;
							const float step = DirectX::XM_2PI / accuracy;
							for (float a = 0.0f; a < DirectX::XM_2PI; a += step) {
								float a_c, a_s, as_c, as_s;
								DirectX::XMScalarSinCos(&a_s, &a_c, a);
								DirectX::XMScalarSinCos(&as_s, &as_c, a + step);

								Vector startPos = Vector(a_c * radius + boundOrigin.x, a_s * radius + boundOrigin.y, boundOrigin.z);
								Vector endPos = Vector(as_c * radius + boundOrigin.x, as_s * radius + boundOrigin.y, boundOrigin.z);

								Vector2D start2d, end2d, boundorigin2d;
								if (!WorldToScreen(startPos, start2d) || !WorldToScreen(endPos, end2d) || !WorldToScreen(boundOrigin, boundorigin2d)) {
									excluded_ents.push_back(i);
									continue;
								}

								s n;
								n.a = start2d;
								n.b = end2d;
								n.c = boundorigin2d;
								valid_molotovs.push_back(n);
							}

							if (!excluded_ents.empty()) {
								for (int v = 0; v < excluded_ents.size(); ++v) {
									auto bbrr = excluded_ents[v];
									if (bbrr == i)
										continue;

									if (!valid_molotovs.empty())
										for (int m = 0; m < valid_molotovs.size(); ++m) {
											auto ba = valid_molotovs[m];
											Render::Engine::FilledTriangle(ba.c, ba.a, ba.b, color.ToRegularColor().OverrideAlpha(45));
											Render::Engine::Line(ba.a, ba.b, color.ToRegularColor().OverrideAlpha(220));
										}
								}
							}
							else {
								if (!valid_molotovs.empty())
									for (int m = 0; m < valid_molotovs.size(); ++m) {
										auto ba = valid_molotovs[m];
										Render::Engine::FilledTriangle(ba.c, ba.a, ba.b, color.ToRegularColor().OverrideAlpha(45));
										Render::Engine::Line(ba.a, ba.b, color.ToRegularColor().OverrideAlpha(220));
									}
							}

							char buf[128] = { };
							sprintf(buf, XorStr("FIRE : %.2fs"), time);
							Render::Engine::RectFilled(Vector2D(new_pos.x - 2, new_pos.y - 15),
								Vector2D(Render::Engine::segoe.size(buf).m_width + 4, Render::Engine::segoe.size(buf).m_height), Color(0, 0, 0, 200));

							Render::Engine::segoe.string(new_pos.x + (Render::Engine::segoe.size(buf).m_width * 0.5f), new_pos.y - 15, Color(255, 0, 0, 220), buf, Render::Engine::ALIGN_CENTER);
						}
						else {
							if (!valid_molotovs.empty())
								valid_molotovs.erase(valid_molotovs.begin() + i);

							if (!excluded_ents.empty())
								excluded_ents.erase(excluded_ents.begin() + i);
						}
					}
				}
			}

			C_SmokeGrenadeProjectile* pSmokeEffect = reinterpret_cast<C_SmokeGrenadeProjectile*>(entity);
			if (pSmokeEffect->GetClientClass()->m_ClassID == CSmokeGrenadeProjectile) {
				const Vector origin = pSmokeEffect->GetAbsOrigin();
				Vector2D screen_origin = Vector2D();

				if (WorldToScreen(origin, screen_origin)) {
					struct s {
						Vector2D a, b;
					};
					std::vector<int> excluded_ents;
					std::vector<s> valid_smokes;
					const auto spawn_time = TICKS_TO_TIME(pSmokeEffect->m_nSmokeEffectTickBegin());
					const auto time = (spawn_time + C_SmokeGrenadeProjectile::GetExpiryTime()) - Interfaces::m_pGlobalVars->curtime;

					static const auto size = Vector2D(70.f, 4.f);

					auto new_pos = Vector2D(screen_origin.x - size.x * 0.5, screen_origin.y - size.y * 0.5);
					if (time > 0.05f) {
						auto radius = 120.f;

						const int accuracy = 25;
						const float step = DirectX::XM_2PI / accuracy;
						for (float a = 0.0f; a < DirectX::XM_2PI; a += step) {
							float a_c, a_s, as_c, as_s;
							DirectX::XMScalarSinCos(&a_s, &a_c, a);
							DirectX::XMScalarSinCos(&as_s, &as_c, a + step);

							Vector startPos = Vector(a_c * radius + origin.x, a_s * radius + origin.y, origin.z + 5);
							Vector endPos = Vector(as_c * radius + origin.x, as_s * radius + origin.y, origin.z + 5);

							Vector2D start2d, end2d;
							if (!WorldToScreen(startPos, start2d) || !WorldToScreen(endPos, end2d)) {
								excluded_ents.push_back(i);
								continue;
							}

							s n;
							n.a = start2d;
							n.b = end2d;
							valid_smokes.push_back(n);
						}

						if (!excluded_ents.empty()) {
							for (int v = 0; v < excluded_ents.size(); ++v) {
								auto bbrr = excluded_ents[v];
								if (bbrr == i)
									continue;

								if (!valid_smokes.empty())
									for (int m = 0; m < valid_smokes.size(); ++m) {
										auto ba = valid_smokes[m];
										Render::Engine::FilledTriangle(screen_origin, ba.a, ba.b, Color(220, 220, 220, 25));
										Render::Engine::Line(ba.a, ba.b, Color(220, 220, 220, 220));
									}
							}
						}
						else {
							if (!valid_smokes.empty())
								for (int m = 0; m < valid_smokes.size(); ++m) {
									auto ba = valid_smokes[m];
									Render::Engine::FilledTriangle(screen_origin, ba.a, ba.b, Color(220, 220, 220, 25));
									Render::Engine::Line(ba.a, ba.b, Color(220, 220, 220, 220));
								}
						}

						char buf[128] = { };
						sprintf(buf, XorStr("SMOKE : %.2fs"), time);
						Render::Engine::RectFilled(Vector2D(new_pos.x - 2, new_pos.y - 15),
							Vector2D(Render::Engine::segoe.size(buf).m_width + 4, Render::Engine::segoe.size(buf).m_height), Color(0, 0, 0, 200));

						Render::Engine::segoe.string(new_pos.x + (Render::Engine::segoe.size(buf).m_width * 0.5f), new_pos.y - 15, Color(0, 230, 255, 180), buf, Render::Engine::ALIGN_CENTER);
					}
					else {
						if (!valid_smokes.empty())
							valid_smokes.erase(valid_smokes.begin() + i);

						if (!excluded_ents.empty())
							excluded_ents.erase(excluded_ents.begin() + i);
					}
				}
			}
		}

		auto player = ToCSPlayer(entity);

		if (ValidPlayer(player) && i <= 64) {
			g_Vars.globals.m_vecTextInfo[i].clear();

			if (Begin(player)) {
				if (g_Vars.esp.skeleton)
					DrawSkeleton(player);

				if (g_Vars.esp.box)
					DrawBox(m_Data.bbox, g_Vars.esp.box_color, player);

				if (g_Vars.esp.health)
					DrawHealthBar(player, m_Data.bbox);

				DrawInfo(player, m_Data.bbox, m_Data.info);

				if (g_Vars.esp.name)
					DrawName(player, m_Data.bbox, m_Data.info);

				if (g_Vars.esp.draw_ammo_bar || g_Vars.esp.draw_lby_bar)
					AmmoBar(player, m_Data.bbox);

				DrawBottomInfo(player, m_Data.bbox, m_Data.info);
			}
		}

		auto rgb_to_int = [](int red, int green, int blue) -> int {
			int r;
			int g;
			int b;

			r = red & 0xFF;
			g = green & 0xFF;
			b = blue & 0xFF;
			return (r << 16 | g << 8 | b);
		};

		if (entity->GetClientClass()->m_ClassID == CFogController) {
			static DWORD dwFogEnable = Engine::Displacement.DT_FogController.m_fog_enable;
			*(byte*)((uintptr_t)entity + dwFogEnable) = g_Vars.esp.fog_effect;

			g_Vars.r_3dsky->SetValue(int(!g_Vars.esp.fog_effect));

			*(bool*)(uintptr_t(entity) + 0xA1D) = g_Vars.esp.fog_effect;
			*(float*)(uintptr_t(entity) + 0x9F8) = 0;
			*(float*)(uintptr_t(entity) + 0x9FC) = g_Vars.esp.fog_distance;
			*(float*)(uintptr_t(entity) + 0xA04) = g_Vars.esp.fog_density / 100.f;
			*(float*)(uintptr_t(entity) + 0xA24) = g_Vars.esp.fog_hdr_scale / 100.f;
			*(int*)(uintptr_t(entity) + 0x9E8) = rgb_to_int((int)(g_Vars.esp.fog_color.b * 255.f), (int)(g_Vars.esp.fog_color.g * 255.f), (int)(g_Vars.esp.fog_color.r * 255.f));
			*(int*)(uintptr_t(entity) + 0x9EC) = rgb_to_int((int)(g_Vars.esp.fog_color.b * 255.f), (int)(g_Vars.esp.fog_color.g * 255.f), (int)(g_Vars.esp.fog_color.r * 255.f));


			continue;
		}

		RenderNades((C_WeaponCSBaseGun*)entity);

		float distance{ };
		distance = !m_LocalPlayer->IsDead() ? m_LocalPlayer->GetAbsOrigin().Distance(entity->m_vecOrigin()) : 0.f;

		float initial_alpha = 255.f;
		const auto clamped_distance = std::clamp<float>(distance - 300.f, 0.f, 510.f);
		initial_alpha = 255.f - (clamped_distance * 0.5f);
		initial_alpha *= 0.70588235294;

		if (!_strcmpi(entity->GetClientClass()->m_pNetworkName, (XorStr("CPlantedC4")))) {
			auto bomb_entity = (C_PlantedC4*)entity;

			static ConVar* mp_c4timer = Interfaces::m_pCvar->FindVar(XorStr("mp_c4timer"));
			static bool bomb_planted_tick_begin = false;
			static float bomb_time_tick_begin = 0.f;

			if (g_Vars.esp.draw_c4_bar && g_Vars.globals.bBombActive) {
				Vector origin = bomb_entity->GetAbsOrigin();
				Vector2D screen_origin = Vector2D();

				if (!bomb_planted_tick_begin) {
					bomb_time_tick_begin = Interfaces::m_pGlobalVars->curtime;
					bomb_planted_tick_begin = true;
				}

				float timer_bomb = 0.f;
				if (bomb_entity->m_flC4Blow() - Interfaces::m_pGlobalVars->curtime > 0.f)
					timer_bomb = bomb_entity->m_flC4Blow() - Interfaces::m_pGlobalVars->curtime;
				else
					timer_bomb = 0.f;

				if (timer_bomb > 0.f) {
					const auto spawn_time = TIME_TO_TICKS(bomb_time_tick_begin);
					const auto factor = timer_bomb / mp_c4timer->GetFloat();

					char subs[64] = { };
					sprintf(subs, XorStr("%.1fs"), timer_bomb);

					char buf[64] = { };
					sprintf(buf, XorStr("C4 - %s"), subs);

					if (factor > 0.f) {
						Math::Clamp(factor, 0.f, 1.0f);

						Vector2D center = Render::GetScreenSize();

						static float flBombTickTime = Interfaces::m_pGlobalVars->realtime;
						static float flRedBombAlpha = 1.f;
						if (g_Vars.globals.bBombTicked) {
							flRedBombAlpha = 1.f;
							flBombTickTime = Interfaces::m_pGlobalVars->realtime;
							g_Vars.globals.bBombTicked = false;
						}

						float flBeepDelta = fabs(Interfaces::m_pGlobalVars->realtime - flBombTickTime);
						bool bRenderRedBomb = (flBeepDelta <= 0.1f && flBeepDelta > 0.f) || g_Vars.globals.bBombTicked;
						if (!bRenderRedBomb) {
							flRedBombAlpha -= (1.0f / 0.1f) * Interfaces::m_pGlobalVars->frametime;
						}

						flRedBombAlpha = std::clamp<float>(flRedBombAlpha, 0.f, 1.f);

						static const auto size_icon = Vector2D(Render::Engine::cs_large.size(XorStr("q")).m_width + 2, 4.f);
						static const auto size = Vector2D(Render::Engine::indi.size(buf).m_width + 2, 4.f);
						Render::Engine::cs_large.string(((center.x * 0.5f) - Render::Engine::indi.size(std::string(" - ") + subs).m_width) + 30, center.y * 0.15f + 2,
							Color::White().OverrideAlpha(180 - (180 * flRedBombAlpha)), XorStr("q"), Render::Engine::ALIGN_LEFT);

						//if( bRenderRedBomb )
						Render::Engine::cs_large.string(((center.x * 0.5f) - Render::Engine::indi.size(std::string(" - ") + subs).m_width) + 30, center.y * 0.15f + 2,
							Color::Red().OverrideAlpha(255 * flRedBombAlpha), XorStr("q"), Render::Engine::ALIGN_LEFT);

						Render::Engine::indi.string((center.x * 0.5f) + size_icon.x, center.y * 0.15f, Color::White().OverrideAlpha(180 - (180 * flRedBombAlpha)), std::string(buf).substr(2), Render::Engine::ALIGN_CENTER);

						//if( bRenderRedBomb )
						Render::Engine::indi.string((center.x * 0.5f) + size_icon.x, center.y * 0.15f, Color::Red().OverrideAlpha(255 * flRedBombAlpha), std::string(buf).substr(2), Render::Engine::ALIGN_CENTER);
					}

					if (WorldToScreen(origin, screen_origin)) {
						static const auto size = Vector2D(Render::Engine::segoe.size(buf).m_width + 2, 4.f);

						auto new_pos = Vector2D(screen_origin.x - size.x * 0.5, screen_origin.y - size.y * 0.5);

						if (factor > 0.f) {
							Math::Clamp(factor, 0.f, 1.0f);
							Render::Engine::RectFilled(new_pos, size, FloatColor(0.f, 0.f, 0.f, 0.58f).ToRegularColor());
							Render::Engine::RectFilled(Vector2D(new_pos.x + 1, new_pos.y + 1), Vector2D((size.x - 1) * factor, size.y - 2), g_Vars.esp.c4_color.ToRegularColor().OverrideAlpha(255 * 0.87f));

							Render::Engine::segoe.string(new_pos.x + (size.x * 0.5f), new_pos.y - 9, g_Vars.esp.c4_color.ToRegularColor().OverrideAlpha(180 * 0.87f), buf, Render::Engine::ALIGN_CENTER);
						}
					}
				}
			}
		}

		if (g_Vars.esp.draw_c4_bar) {
			if (entity->m_hOwnerEntity() == -1 &&
				entity->GetClientClass()->m_ClassID == CC4) {

				Vector2D out;
				if (WorldToScreen(entity->GetAbsOrigin(), out)) {
					Render::Engine::segoe.string(out.x + 2, out.y, g_Vars.esp.c4_color.ToRegularColor().OverrideAlpha(180 * 0.87f), XorStr("C4"));
				}
			}
		}

		if ((g_Vars.esp.dropped_weapons || g_Vars.esp.dropped_weapons_ammo) && initial_alpha && !m_LocalPlayer->IsDead()) {
			if (!entity->IsWeapon() || entity->m_hOwnerEntity() != -1 ||
				entity->GetClientClass()->m_ClassID == CC4 ||
				entity->GetClientClass()->m_ClassID == CPlantedC4)
				continue;

			auto weapon = reinterpret_cast<C_WeaponCSBaseGun*>(entity);
			if (!weapon)
				continue;

			Vector2D out;
			if (!WorldToScreen(weapon->GetAbsOrigin(), out))
				continue;

			auto weapondata = weapon->GetCSWeaponData();
			if (!weapondata.IsValid())
				continue;

			if (!weapon->m_iItemDefinitionIndex())
				continue;

			std::wstring localized = Interfaces::m_pLocalize->Find(weapondata->m_szHudName);
			std::string name{ localized.begin(), localized.end() };
			std::transform(name.begin(), name.end(), name.begin(), ::toupper);

			if (name.empty())
				continue;

			if (g_Vars.esp.dropped_weapons) {
				Render::Engine::segoe.string(out.x + 2, out.y, g_Vars.esp.dropped_weapons_color.ToRegularColor().OverrideAlpha(static_cast<int>(initial_alpha)), name);
			}

			if (g_Vars.esp.dropped_weapons_ammo) {
				auto clip = weapon->m_iClip1();
				if (clip > 0) {
					const auto TextSize = Render::Engine::segoe.size(name);

					const auto MaxClip = weapondata->m_iMaxClip;
					auto Width = TextSize.m_width;

					Width *= clip;
					Width /= MaxClip;

					Render::Engine::RectFilled(Vector2D(out.x, out.y) + Vector2D(1, 9), Vector2D(TextSize.m_width + 1, 4),
						FloatColor(0.f, 0.f, 0.f, (initial_alpha / 255.f) * 0.58f).ToRegularColor());


					Render::Engine::RectFilled(Vector2D(out.x, out.y) + Vector2D(2, 10), Vector2D(Width - 1, 2),
						g_Vars.esp.dropped_weapons_color.ToRegularColor().OverrideAlpha(static_cast<int>(initial_alpha)));

					if (clip <= static_cast<int>(MaxClip * 0.75)) {
						Render::Engine::pixel_reg.string(out.x + Width, out.y + 8, Color::White().OverrideAlpha(static_cast<int>(initial_alpha)), std::to_string(clip));
					}
				}
			}
		}
	}
}

void CEsp::SetAlpha(int idx) {
	m_bAlphaFix[idx] = true;
}

float CEsp::GetAlpha(int idx) {
	return m_flAlpha[idx];
}

#include "../Rage/Resolver.hpp"
void CEsp::AmmoBar(C_CSPlayer* player, BBox_t bbox) {
	if (!player)
		return;

	C_WeaponCSBaseGun* pWeapon = (C_WeaponCSBaseGun*)player->m_hActiveWeapon().Get();

	if (!pWeapon)
		return;

	auto pWeaponData = pWeapon->GetCSWeaponData();
	if (!pWeaponData.IsValid())
		return;

	int index = 0;

	if (g_Vars.esp.draw_ammo_bar) {
		auto animLayer = player->m_AnimOverlay().Element(1);
		if (animLayer.m_pOwner && pWeaponData->m_iWeaponType != WEAPONTYPE_GRENADE && pWeaponData->m_iWeaponType != WEAPONTYPE_KNIFE && pWeaponData->m_iWeaponType != WEAPONTYPE_C4) {
			index += 6;

			auto activity = player->GetSequenceActivity(animLayer.m_nSequence);

			int current = pWeapon->m_iClip1();
			int max = pWeaponData->m_iMaxClip;
			bool reloading = activity == 967 && animLayer.m_flWeight != 0.f;
			int reload_percentage = reloading ? std::ceil(100 * animLayer.m_flCycle) : 100;
			float scale;

			// check for reload.
			if (reloading)
				scale = animLayer.m_flCycle;

			// not reloading.
			// make the division of 2 ints produce a float instead of another int.
			else
				scale = max != -1 ? (float)current / max : 1.f;

			// relative to bar.
			int bar = (int)std::round((bbox.w - 2) * scale);

			// draw.
			Render::Engine::RectFilled(bbox.x, bbox.y + bbox.h + 2, bbox.w, 4, Color(0, 0, 0, 180 * this->m_flAlpha[player->EntIndex()]));

			Color clr = g_Vars.esp.ammo_color.ToRegularColor();
			clr.RGBA[3] *= this->m_flAlpha[player->EntIndex()];

			Render::Engine::Rect(bbox.x + 1, bbox.y + bbox.h + 3, bar, 2, clr);

			// less then a 5th of the bullets left.
			if (current < max || reloading) {
				Render::Engine::pixel_reg.string(bbox.x + bar, bbox.y + bbox.h, Color(255, 255, 255, 180 * this->m_flAlpha[player->EntIndex()]), std::to_string(reloading ? reload_percentage : current).append(reloading ? XorStr("%") : XorStr("")), Render::Engine::ALIGN_CENTER);
			}
		}
	}

	auto local = C_CSPlayer::GetLocalPlayer();

	if (g_Vars.esp.draw_lby_bar && !local->IsDead() && Engine::g_ResolverData[player->EntIndex()].m_bPredictingUpdates) {
		int current = pWeapon->m_iClip1();
		int max = pWeaponData->m_iMaxClip;
		float scale;

		float flUpdateTime = Engine::g_ResolverData[player->EntIndex()].m_flNextBodyUpdate - player->m_flAnimationTime();

		// check for pred.
		scale = (1.1f - flUpdateTime) / 1.1f;

		// relative to bar.
		int bar = std::clamp((int)std::round((bbox.w - 2) * scale), 0, bbox.w - 2);

		// draw.
		Render::Engine::RectFilled(bbox.x, bbox.y + bbox.h + 2 + index, bbox.w, 4, Color(0, 0, 0, 180 * this->m_flAlpha[player->EntIndex()]));

		Color clr = g_Vars.esp.lby_color.ToRegularColor();
		clr.RGBA[3] *= this->m_flAlpha[player->EntIndex()];

		Render::Engine::Rect(bbox.x + 1, bbox.y + bbox.h + 3 + index, bar, 2, clr);
	}
}

void CEsp::RenderNades(C_WeaponCSBaseGun* nade) {
	if (!g_Vars.esp.nades)
		return;

	const model_t* model = nade->GetModel();
	if (!model)
		return;

	studiohdr_t* hdr = Interfaces::m_pModelInfo->GetStudiomodel(model);
	if (!hdr)
		return;

	int item_definition = 0;
	bool dont_render = false;
	C_SmokeGrenadeProjectile* pSmokeEffect = nullptr;
	C_Inferno* pMolotov = nullptr;
	Color Nadecolor;
	std::string Name = hdr->szName;
	switch (nade->GetClientClass()->m_ClassID) {
	case ClassId_t::CBaseCSGrenadeProjectile:
		if (Name[16] == 's') {
			Name = XorStr("FLASH");
			item_definition = WEAPON_FLASHBANG;
		}
		else {
			Name = XorStr("FRAG");
			item_definition = WEAPON_HEGRENADE;
		}
		break;
	case ClassId_t::CSmokeGrenadeProjectile:
		Name = XorStr("SMOKE");
		item_definition = WEAPON_SMOKE;
		pSmokeEffect = reinterpret_cast<C_SmokeGrenadeProjectile*>(nade);
		if (pSmokeEffect) {
			const auto spawn_time = TICKS_TO_TIME(pSmokeEffect->m_nSmokeEffectTickBegin());
			const auto time = (spawn_time + C_SmokeGrenadeProjectile::GetExpiryTime()) - Interfaces::m_pGlobalVars->curtime;
			const auto factor = ((spawn_time + C_SmokeGrenadeProjectile::GetExpiryTime()) - Interfaces::m_pGlobalVars->curtime) / C_SmokeGrenadeProjectile::GetExpiryTime();

			if (factor > 0.0f)
				dont_render = true;
		}
		else {
			dont_render = false;
		}
		break;
	case ClassId_t::CMolotovProjectile:
		Name = XorStr("FIRE");
		// bich
		if (nade && (nade->m_hOwnerEntity().Get()) && ((C_CSPlayer*)(nade->m_hOwnerEntity().Get()))) {
			item_definition = ((C_CSPlayer*)(nade->m_hOwnerEntity().Get()))->m_iTeamNum() == TEAM_CT ? WEAPON_FIREBOMB : WEAPON_MOLOTOV;
		}
		pMolotov = reinterpret_cast<C_Inferno*>(nade);
		if (pMolotov) {
			const auto spawn_time = pMolotov->m_flSpawnTime();
			const auto time = ((spawn_time + C_Inferno::GetExpiryTime()) - Interfaces::m_pGlobalVars->curtime);

			if (time <= 0.05f)
				dont_render = true;
		}
		else {
			dont_render = false;
		}
		break;
	case ClassId_t::CDecoyProjectile:
		Name = XorStr("DECOY");
		item_definition = WEAPON_DECOY;
		break;
	default:
		return;
	}

	Vector2D points_transformed[8];
	BBox_t size;

	if (!GetBBox(nade, points_transformed, size) || dont_render)
		return;

	Render::Engine::segoe.string(size.x, size.y - 2, Color(255, 255, 255, 220), Name.c_str(), Render::Engine::ALIGN_CENTER);
}

void CEsp::DrawBox(BBox_t bbox, const FloatColor& clr, C_CSPlayer* player) {
	if (!player)
		return;

	auto color = clr;
	color.a *= (m_flAlpha[player->EntIndex()]);

	FloatColor outline = FloatColor(0.0f, 0.0f, 0.0f, color.a * 0.68f);
	//if( g_Vars.esp.box_type == 0 ) {
	Render::Engine::Rect(bbox.x - 1, bbox.y - 1, bbox.w + 2, bbox.h + 2, Color(0, 0, 0, 180 * m_flAlpha[player->EntIndex()]));
	Render::Engine::Rect(bbox.x + 1, bbox.y + 1, bbox.w - 2, bbox.h - 2, Color(0, 0, 0, 180 * m_flAlpha[player->EntIndex()]));
	Render::Engine::Rect(bbox.x, bbox.y, bbox.w, bbox.h, color.ToRegularColor());
	//}
}

void CEsp::DrawHealthBar(C_CSPlayer* player, BBox_t bbox) {
	int y = bbox.y + 1;
	int h = bbox.h - 2;

	// retarded servers that go above 100 hp..
	int hp = std::min(100, player->m_iHealth());

	// calculate hp bar color.
	int r = std::min((510 * (100 - hp)) / 100, 255);
	int g = std::min((510 * hp) / 100, 255);

	// get hp bar height.
	int fill = (int)std::round(hp * h / 100.f);

	// render background.
	Render::Engine::RectFilled(bbox.x - 6, y - 1, 4, h + 2, Color(10, 10, 10, 180 * GetAlpha(player->EntIndex())));

	// render actual bar.
	Render::Engine::Rect(bbox.x - 5, y + h - fill, 2, fill, g_Vars.esp.health_override ? g_Vars.esp.health_color.ToRegularColor().OverrideAlpha(210 * GetAlpha(player->EntIndex()), true)
		: Color(r, g, 0, 210 * GetAlpha(player->EntIndex())));

	// if hp is below max, draw a string.
	if (hp < 100)
		Render::Engine::pixel_reg.string(bbox.x - 5, y + (h - fill) - 5, Color(255, 255, 255, 200 * GetAlpha(player->EntIndex())), std::to_string(hp), Render::Engine::ALIGN_CENTER);
}

void CEsp::DrawInfo(C_CSPlayer* player, BBox_t bbox, player_info_t player_info) {
	auto animState = player->m_PlayerAnimState();
	if (!animState)
		return;

	auto color = FloatColor(0, 150, 255, (int)(180 * m_flAlpha[player->EntIndex()]));
	color.a *= m_flAlpha[player->EntIndex()];

	auto anim_data = Engine::AnimationSystem::Get()->GetAnimationData(player->m_entIndex);
	auto lag_data = Engine::LagCompensation::Get()->GetLagData(player->m_entIndex);


	if (player->m_bIsScoped() && g_Vars.esp.draw_scoped)
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(0, 150, 255, (int)(180 * m_flAlpha[player->EntIndex()])), XorStr("ZOOM"));

	if (g_Vars.esp.draw_money)
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(133, 198, 22, (int)(180 * m_flAlpha[player->EntIndex()])), XorStr("$") + std::to_string(player->m_iAccount()));

	if (g_Vars.esp.draw_armor && player->m_ArmorValue() > 0) {
		std::string name = player->m_bHasHelmet() ? XorStr("HK") : XorStr("K");
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(255, 255, 255, (int)(180 * m_flAlpha[player->EntIndex()])), name.c_str());
	}

	if (g_Vars.esp.draw_defusing && player->m_bHasDefuser()) {
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(105, 218, 204, (int)(180 * m_flAlpha[player->EntIndex()])), XorStr("KIT"));
	}

	if (g_Vars.esp.draw_flashed && player->m_flFlashDuration() > 1.f)
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(255, 216, 0, (int)(180 * m_flAlpha[player->EntIndex()])), XorStr("FLASH"));

	if (g_Vars.esp.draw_defusing && player->m_bIsDefusing()) {
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(235, 82, 82, (int)(180 * m_flAlpha[player->EntIndex()])), XorStr("DEF"));
	}

	if (player->m_bIsDefusing()) {
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(235, 82, 82, (int)(180 * m_flAlpha[player->EntIndex()])), XorStr("DEF"));
	}



	int ping = round((*Interfaces::m_pPlayerResource.Xor())->GetPlayerPing(player->EntIndex()));



	if (ping > 50)
	{

		float ping_percent = std::clamp(ping, 0, 750) / 750.f;

		Color ping_flag(255, 255 - ((ping_percent) * 255), 255 - ((ping_percent) * 255));


		ping_flag.RGBA[3] = (int)(180 * m_flAlpha[player->EntIndex()]);

		FloatColor Ping_Flag_Last = FloatColor(ping_flag.RGBA[0], ping_flag.RGBA[1], ping_flag.RGBA[2], ping_flag.RGBA[3]);
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(Ping_Flag_Last, "" + std::to_string(ping) + " ms");
	}


	if (g_Vars.esp.draw_distance) {
		C_CSPlayer* local = C_CSPlayer::GetLocalPlayer();
		if (local && !local->IsDead()) {
			auto round_to_multiple = [&](int in, int multiple) {
				const auto ratio = static_cast<double>(in) / multiple;
				const auto iratio = std::lround(ratio);
				return static_cast<int>(iratio * multiple);
			};

			float distance = local->m_vecOrigin().Distance(player->m_vecOrigin());

			auto meters = distance * 0.0254f;
			auto feet = meters * 3.281f;

			std::string str = std::to_string(round_to_multiple(feet, 5)) + XorStr(" FT");
			g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(255, 255, 255, (int)(180 * m_flAlpha[player->EntIndex()])), str.c_str());
		}
	}

	auto weapons = player->m_hMyWeapons();
	for (size_t i = 0; i < 48; ++i) {
		auto weapon_handle = weapons[i];
		if (!weapon_handle.IsValid())
			break;

		auto weapon = (C_BaseCombatWeapon*)weapon_handle.Get();
		if (!weapon)
			continue;

		auto definition_index = weapon->m_Item().m_iItemDefinitionIndex();

		if (definition_index == WEAPON_C4 && g_Vars.esp.draw_bombc4)
			g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(255, 0, 0, (int)(180 * m_flAlpha[player->EntIndex()])), XorStr("BOMB"));
	}

	auto pWeapon = (C_WeaponCSBaseGun*)player->m_hActiveWeapon().Get();
	if (g_Vars.esp.draw_grenade_pin && pWeapon) {
		auto pWeaponData = pWeapon->GetCSWeaponData();
		if (pWeaponData.IsValid()) {
			if (pWeaponData->m_iWeaponType == WEAPONTYPE_GRENADE && pWeapon->m_bPinPulled()) {
				g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(255, 0, 0, (int)(180 * m_flAlpha[player->EntIndex()])), XorStr("PIN"));
			}
		}
	}

	auto local = C_CSPlayer::GetLocalPlayer();
	if (!local->IsDead() && anim_data && lag_data.IsValid() && g_Vars.esp.draw_resolver) {
		if (!anim_data->m_AnimationRecord.empty()) {
			auto current = &anim_data->m_AnimationRecord.front();
			if (current) {
				g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(0, 255, 255, (int)(180 * m_flAlpha[player->EntIndex()])), current->m_resolver_mode);
			}
		}
	}

	if (local && anim_data && !player->IsDormant() && lag_data.IsValid() && anim_data->m_AnimationRecord.size() >= 2 && g_Vars.esp.draw_layer_info) {
		int layer = g_Vars.esp.layer + 1;
		C_AnimationLayer* current_layer = &player->m_AnimOverlay()[layer];

		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(255, 255, 255, (int)(180 * m_flAlpha[player->EntIndex()])), std::string("C ") + std::to_string(current_layer->m_flCycle));
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(255, 255, 255, (int)(180 * m_flAlpha[player->EntIndex()])), std::string("W ") + std::to_string(current_layer->m_flWeight));
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(255, 255, 255, (int)(180 * m_flAlpha[player->EntIndex()])), std::string("P ") + std::to_string(current_layer->m_flPlaybackRate));
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(255, 255, 255, (int)(180 * m_flAlpha[player->EntIndex()])), std::string("B ") + std::to_string(current_layer->m_flBlendIn));
		g_Vars.globals.m_vecTextInfo[player->EntIndex()].emplace_back(FloatColor(50, 255, 50, (int)(180 * m_flAlpha[player->EntIndex()])), std::to_string(player->GetSequenceActivity(current_layer->m_nSequence)));
	}

	int i = 0;
	for (auto text : g_Vars.globals.m_vecTextInfo[player->EntIndex()]) {
		Render::Engine::pixel_reg.string(bbox.x + bbox.w + 2, bbox.y + i, text.first.ToRegularColor(), text.second.c_str());
		i += (Render::Engine::pixel_reg.m_size.m_height - 1);
	}
}

std::map<int, char> weapon_icons = {
	{ WEAPON_DEAGLE, 'A' },
	{ WEAPON_ELITE, 'B' },
	{ WEAPON_FIVESEVEN, 'C' },
	{ WEAPON_GLOCK, 'D' },
	{ WEAPON_P2000, 'E' },
	{ WEAPON_P250, 'F' },
	{ WEAPON_USPS, 'G' },
	{ WEAPON_TEC9, 'H' },
	{ WEAPON_CZ75A, 'I' },
	{ WEAPON_REVOLVER, 'J' },
	{ WEAPON_MAC10, 'K' },
	{ WEAPON_UMP45, 'L' },
	{ WEAPON_BIZON, 'M' },
	{ WEAPON_MP7, 'N' },
	{ WEAPON_MP9, 'O' },
	{ WEAPON_P90, 'P' },
	{ WEAPON_GALIL, 'Q' },
	{ WEAPON_FAMAS, 'R' },
	{ WEAPON_M4A4, 'S' },
	{ WEAPON_M4A1S, 'T' },
	{ WEAPON_AUG, 'U' },
	{ WEAPON_SG553, 'V' },
	{ WEAPON_AK47, 'W' },
	{ WEAPON_G3SG1, 'X' },
	{ WEAPON_SCAR20, 'Y' },
	{ WEAPON_AWP, 'Z' },
	{ WEAPON_SSG08, 'a' },
	{ WEAPON_XM1014, 'b' },
	{ WEAPON_SAWEDOFF, 'c' },
	{ WEAPON_MAG7, 'd' },
	{ WEAPON_NOVA, 'e' },
	{ WEAPON_NEGEV, 'f' },
	{ WEAPON_M249, 'g' },
	{ WEAPON_ZEUS, 'h' },
	{ WEAPON_KNIFE_T, 'i' },
	{ WEAPON_KNIFE_CT, 'j' },
	{ WEAPON_KNIFE_FALCHION, '0' },
	{ WEAPON_KNIFE_BAYONET, '1' },
	{ WEAPON_KNIFE_FLIP, '2' },
	{ WEAPON_KNIFE_GUT, '3' },
	{ WEAPON_KNIFE_KARAMBIT, '4' },
	{ WEAPON_KNIFE_M9_BAYONET, '5' },
	{ WEAPON_KNIFE_HUNTSMAN, '6' },
	{ WEAPON_KNIFE_BOWIE, '7' },
	{ WEAPON_KNIFE_BUTTERFLY, '8' },
	{ WEAPON_FLASHBANG, 'k' },
	{ WEAPON_HEGRENADE, 'l' },
	{ WEAPON_SMOKE, 'm' },
	{ WEAPON_MOLOTOV, 'n' },
	{ WEAPON_DECOY, 'o' },
	{ WEAPON_FIREBOMB, 'p' },
	{ WEAPON_C4, 'q' },
};

std::string GetWeaponIcon(const int id) {
	auto search = weapon_icons.find(id);
	if (search != weapon_icons.end())
		return std::string(&search->second, 1);

	return "";
}

std::string GetLocalizedName(CCSWeaponInfo* wpn_data) {
	if (!wpn_data)
		return XorStr("ERROR");

	return Math::WideToMultiByte(Interfaces::m_pLocalize->Find(wpn_data->m_szHudName));
}

void CEsp::DrawBottomInfo(C_CSPlayer* player, BBox_t bbox, player_info_t player_info) {
	std::vector<std::pair<Color, std::pair<std::string, Render::Engine::Font>>> m_vecTextInfo;

	auto pWeapon = (C_WeaponCSBaseGun*)(player->m_hActiveWeapon().Get());
	auto color = g_Vars.esp.weapon_color.ToRegularColor().OverrideAlpha(180, true);
	color.RGBA[3] *= m_flAlpha[player->EntIndex()];

	auto color_icon = g_Vars.esp.weapon_icon_color.ToRegularColor().OverrideAlpha(180, true);
	color_icon.RGBA[3] *= m_flAlpha[player->EntIndex()];

	if (!pWeapon)
		return;

	auto pWeaponData = pWeapon->GetCSWeaponData();
	if (!pWeaponData.IsValid())
		return;

	float i = 2.f;

	if (g_Vars.esp.draw_ammo_bar && (pWeaponData->m_iWeaponType != WEAPONTYPE_GRENADE && pWeaponData->m_iWeaponType != WEAPONTYPE_KNIFE && pWeaponData->m_iWeaponType != WEAPONTYPE_C4))
		i += 6.f;

	if (g_Vars.esp.draw_lby_bar && Engine::g_ResolverData[player->EntIndex()].m_bPredictingUpdates)
		i += 6.f;

	if (g_Vars.esp.weapon_icon) {
		m_vecTextInfo.emplace_back(color_icon, std::pair{ GetWeaponIcon(pWeapon->m_iItemDefinitionIndex()), Render::Engine::cs });
	}

	if (g_Vars.esp.weapon) {
		std::string name{ GetLocalizedName(pWeaponData.Xor()) };

		std::transform(name.begin(), name.end(), name.begin(), ::toupper);

		m_vecTextInfo.emplace_back(color, std::pair{ name, Render::Engine::pixel_reg });
	}

	for (auto text : m_vecTextInfo) {
		if (text.second.second.m_handle == Render::Engine::pixel_reg.m_handle) {
			text.second.second.string(bbox.x + bbox.w / 2, (bbox.y + bbox.h - 2) + i, text.first, text.second.first.c_str(), Render::Engine::ALIGN_CENTER);
		}
		else {
			text.second.second.string(bbox.x + bbox.w / 2, bbox.y + bbox.h + i, text.first, text.second.first.c_str(), Render::Engine::ALIGN_CENTER);
		}

		i += text.second.second.m_size.m_height;
	}
}

void CEsp::DrawName(C_CSPlayer* player, BBox_t bbox, player_info_t player_info) {
	// fix retards with their namechange meme 
	// the point of this is overflowing unicode compares with hardcoded buffers, good hvh strat
	std::string name{ std::string(player_info.szName).substr(0, 24) };

	//#if defined (DEV)
	//	name.append( XorStr( " (" ) ).append( std::to_string( player->m_entIndex ) ).append( XorStr( ")" ) );
	//#endif

	Color clr = g_Vars.esp.name_color.ToRegularColor().OverrideAlpha(180, true);
	clr.RGBA[3] *= m_flAlpha[player->EntIndex()];

	Render::Engine::esp.string(bbox.x + bbox.w / 2, bbox.y - Render::Engine::segoe.m_size.m_height - 1, clr, name, Render::Engine::ALIGN_CENTER);

}

void CEsp::DrawSkeleton(C_CSPlayer* player) {
	auto model = player->GetModel();
	if (!model)
		return;

	if (player->IsDormant())
		return;

	auto* hdr = Interfaces::m_pModelInfo->GetStudiomodel(model);
	if (!hdr)
		return;

	// render skeleton
	Vector2D bone1, bone2;
	for (size_t n{ }; n < hdr->numbones; ++n) {
		auto* bone = hdr->pBone(n);
		if (!bone || !(bone->flags & 256) || bone->parent == -1) {
			continue;
		}

		auto BonePos = [&](int n) -> Vector {
			return Vector(
				player->m_CachedBoneData().m_Memory.m_pMemory[n][0][3],
				player->m_CachedBoneData().m_Memory.m_pMemory[n][1][3],
				player->m_CachedBoneData().m_Memory.m_pMemory[n][2][3]
			);
		};

		if (!WorldToScreen(BonePos(n), bone1) || !WorldToScreen(BonePos(bone->parent), bone2)) {
			continue;
		}

		auto color = g_Vars.esp.skeleton_color.ToRegularColor().OverrideAlpha(180, true);
		color.RGBA[3] *= m_flAlpha[player->EntIndex()];

		Render::Engine::Line(bone1, bone2, color);
	}
}

void CEsp::DrawHitSkeleton() {
	auto pLocal = C_CSPlayer::GetLocalPlayer();

	if (!g_Vars.globals.HackIsReady || !Interfaces::m_pEngine->IsConnected() || !Interfaces::m_pEngine->IsInGame() || !pLocal) {
		m_Hitmatrix.clear();
		return;
	}

	if (m_Hitmatrix.empty())
		return;

	Vector2D bone1, bone2;
	for (size_t i{ }; i < m_Hitmatrix.size(); ++i) {
		auto delta = Interfaces::m_pGlobalVars->realtime - m_Hitmatrix[i].m_flTime;
		if (delta > 0.0f && delta < 1.0f) {
			m_Hitmatrix[i].m_flAlpha -= delta;
		}

		if (m_Hitmatrix[i].m_flAlpha <= 0.0f || !m_Hitmatrix[i].pBoneToWorld || !m_Hitmatrix[i].m_pEntity->GetModel() || !m_Hitmatrix[i].m_pEntity) {
			continue;
		}

		auto* model = Interfaces::m_pModelInfo->GetStudiomodel(m_Hitmatrix[i].m_pEntity->GetModel());
		if (!model) {
			continue;
		}

		// render hurt skeleton
		for (size_t n{ }; n < model->numbones; ++n) {
			auto* bone = model->pBone(n);
			if (!bone || !(bone->flags & 256) || bone->parent == -1) {
				continue;
			}

			auto BonePos = [&](int n) -> Vector {
				return Vector(
					m_Hitmatrix[i].pBoneToWorld[n][0][3],
					m_Hitmatrix[i].pBoneToWorld[n][1][3],
					m_Hitmatrix[i].pBoneToWorld[n][2][3]
				);
			};

			if (!WorldToScreen(BonePos(n), bone1) || !WorldToScreen(BonePos(bone->parent), bone2)) {
				continue;
			}

			Render::Engine::Line(bone1, bone2, g_Vars.esp.hitskeleton_color.ToRegularColor().OverrideAlpha(220 * m_Hitmatrix[i].m_flAlpha, true));
		}
	}
}

void CEsp::Offscreen() {
	C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer();

	if (!LocalPlayer)
		return;

	QAngle viewangles;
	int width = Render::GetScreenSize().x, height = Render::GetScreenSize().y;

	Interfaces::m_pEngine.Xor()->GetViewAngles(viewangles);

	auto rotate_arrow = [](std::array< Vector2D, 3 >& points, float rotation) {
		const auto points_center = (points.at(0) + points.at(1) + points.at(2)) / 3;
		for (auto& point : points) {
			point -= points_center;

			const auto temp_x = point.x;
			const auto temp_y = point.y;

			const auto theta = DEG2RAD(rotation);
			const auto c = cos(theta);
			const auto s = sin(theta);

			point.x = temp_x * c - temp_y * s;
			point.y = temp_x * s + temp_y * c;

			point += points_center;
		}
	};

	auto m_width = Render::GetScreenSize().x, m_height = Render::GetScreenSize().y;
	for (auto i = 1; i <= Interfaces::m_pGlobalVars->maxClients; i++) {
		auto entity = C_CSPlayer::GetPlayerByIndex(i);
		if (!entity || !entity->IsPlayer() || entity == LocalPlayer || entity->IsDormant() || entity->IsDead()
			|| (g_Vars.esp.team_check && entity->m_iTeamNum() == LocalPlayer->m_iTeamNum()))
			continue;

		// get the player's center screen position.
		auto target_pos = entity->GetAbsOrigin();
		Vector2D screen_pos;
		auto is_on_screen = WorldToScreen(target_pos, screen_pos);

		// give some extra room for screen position to be off screen.
		auto leeway_x = m_width / 18.f;
		auto leeway_y = m_height / 18.f;

		if (!is_on_screen
			|| screen_pos.x < -leeway_x
			|| screen_pos.x >(m_width + leeway_x)
			|| screen_pos.y < -leeway_y
			|| screen_pos.y >(m_height + leeway_y)) {

			const auto screen_center = Vector2D(width * .5f, height * .5f);
			const auto angle_yaw_rad = DEG2RAD(viewangles.y - Math::CalcAngle(LocalPlayer->GetEyePosition(), entity->GetAbsOrigin(), true).y - 90);

			float radius = std::max(10.f, g_Vars.esp.offscren_distance);
			float size = std::max(5.f, g_Vars.esp.offscren_size);

			const auto new_point_x = screen_center.x + ((((width - (size * 3)) * .5f) * (radius / 100.0f)) * cos(angle_yaw_rad)) + (int)(6.0f * (((float)size - 4.f) / 16.0f));
			const auto new_point_y = screen_center.y + ((((height - (size * 3)) * .5f) * (radius / 100.0f)) * sin(angle_yaw_rad));

			std::array< Vector2D, 3 >points{ Vector2D(new_point_x - size, new_point_y - size),
				Vector2D(new_point_x + size, new_point_y),
				Vector2D(new_point_x - size, new_point_y + size) };

			rotate_arrow(points, viewangles.y - Math::CalcAngle(LocalPlayer->GetEyePosition(), entity->GetAbsOrigin(), true).y - 90);

			std::array< Vertex_t, 3 >vertices{ Vertex_t(points.at(0)), Vertex_t(points.at(1)), Vertex_t(points.at(2)) };
			static int texture_id = Interfaces::m_pSurface.Xor()->CreateNewTextureID(true);
			static unsigned char buf[4] = { 255, 255, 255, 255 };

			Color clr = g_Vars.esp.offscreen_color.ToRegularColor();

			// fill
			Interfaces::m_pSurface.Xor()->DrawSetColor(clr.r(), clr.g(), clr.b(), (clr.a() * 0.4f) * GetAlpha(i));
			Interfaces::m_pSurface.Xor()->DrawSetTexture(texture_id);
			Interfaces::m_pSurface.Xor()->DrawTexturedPolygon(3, vertices.data());

			// outline
			Interfaces::m_pSurface.Xor()->DrawSetColor(clr.r(), clr.g(), clr.b(), (clr.a()) * GetAlpha(i));
			Interfaces::m_pSurface.Xor()->DrawSetTexture(texture_id);
			Interfaces::m_pSurface.Xor()->DrawTexturedPolyLine(vertices.data(), 3);
		}
	}
}

void CEsp::OverlayInfo() {
	auto local = C_CSPlayer::GetLocalPlayer();
	if (!local)
		return;

	C_WeaponCSBaseGun* weapon = (C_WeaponCSBaseGun*)local->m_hActiveWeapon().Get();
	if (!weapon)
		return;

	if (g_Vars.esp.offscren_enabled)
		Offscreen();

	PenetrateCrosshair(Render::GetScreenSize() / 2);
}

bool CEsp::GetBBox(C_BaseEntity* entity, Vector2D screen_points[], BBox_t& outRect) {
	BBox_t rect{ };
	auto collideable = entity->GetCollideable();

	if (!collideable)
		return false;

	if (entity->IsPlayer()) {
		Vector origin, mins, maxs;
		Vector2D bottom, top;

		// get interpolated origin.
		origin = entity->GetAbsOrigin();

		// get hitbox bounds.
		entity->ComputeHitboxSurroundingBox(&mins, &maxs);

		// correct x and y coordinates.
		mins = { origin.x, origin.y, mins.z };
		maxs = { origin.x, origin.y, maxs.z + 8.f };

		if (!WorldToScreen(mins, bottom) || !WorldToScreen(maxs, top))
			return false;

		// state the box bounds
		outRect.h = bottom.y - top.y;
		outRect.w = outRect.h / 2.f;
		outRect.x = bottom.x - (outRect.w / 2.f);
		outRect.y = bottom.y - outRect.h;

		return true;
	}
	else {
		auto min = collideable->OBBMins();
		auto max = collideable->OBBMaxs();

		const matrix3x4_t& trans = entity->m_rgflCoordinateFrame();

		Vector points[] =
		{
			Vector(min.x, min.y, min.z),
			Vector(min.x, max.y, min.z),
			Vector(max.x, max.y, min.z),
			Vector(max.x, min.y, min.z),
			Vector(max.x, max.y, max.z),
			Vector(min.x, max.y, max.z),
			Vector(min.x, min.y, max.z),
			Vector(max.x, min.y, max.z)
		};

		for (int i = 0; i < 8; i++) {
			points[i] = points[i].Transform(trans);
		}

		for (int i = 0; i < 8; i++)
			if (!WorldToScreen(points[i], screen_points[i]))
				return false;

		auto left = screen_points[0].x;
		auto top = screen_points[0].y;
		auto right = screen_points[0].x;
		auto bottom = screen_points[0].y;

		for (int i = 1; i < 8; i++) {
			if (left > screen_points[i].x)
				left = screen_points[i].x;
			if (top < screen_points[i].y)
				top = screen_points[i].y;
			if (right < screen_points[i].x)
				right = screen_points[i].x;
			if (bottom > screen_points[i].y)
				bottom = screen_points[i].y;
		}

		left = std::ceilf(left);
		top = std::ceilf(top);
		right = std::floorf(right);
		bottom = std::floorf(bottom);

		// state the box bounds.
		outRect.x = left;
		outRect.y = top;
		outRect.w = right - left;
		outRect.h = (bottom - top);
		return true;
	}

	return false;
}

Encrypted_t<IEsp> IEsp::Get() {
	static CEsp instance;
	return &instance;
}