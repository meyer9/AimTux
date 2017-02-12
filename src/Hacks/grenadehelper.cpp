#include "grenadehelper.h"

bool shotLastTick = false;
pstring Settings::GrenadeHelper::actMapName = pstring();
bool Settings::GrenadeHelper::enabled = true;
bool Settings::GrenadeHelper::onlyMatchingInfos = true;
bool Settings::GrenadeHelper::aimAssist = false;
char Settings::GrenadeHelper::inputName[20] = {};
bool Settings::GrenadeHelper::inputJump = false;
ImColor Settings::GrenadeHelper::aimLine = ImColor(200, 200, 200, 255);
ImColor Settings::GrenadeHelper::aimDot = ImColor(10, 10, 200, 255);
ImColor Settings::GrenadeHelper::infoHE = ImColor(7, 183, 7, 255);
ImColor Settings::GrenadeHelper::infoMolotov = ImColor(236, 0, 0, 255);
ImColor Settings::GrenadeHelper::infoSmoke = ImColor(172, 172, 172, 255);
ImColor Settings::GrenadeHelper::infoFlash = ImColor(255, 255, 0, 255);

std::vector<GrenadeInfo> Settings::GrenadeHelper::grenadeInfos = {};

GrenadeType getGrenadeType(C_BaseCombatWeapon* wpn)
{
	if (!strcmp(wpn->GetCSWpnData()->szClassName, "weapon_hegrenade"))
		return GrenadeType::HEGRENADE;
	if (!strcmp(wpn->GetCSWpnData()->szClassName, "weapon_smokegrenade"))
		return GrenadeType::SMOKE;
	if (!strcmp(wpn->GetCSWpnData()->szClassName, "weapon_flashbang"))
		return GrenadeType::FLASH;
	return GrenadeType::MOLOTOV;// "weapon_molotov", "weapon_incgrenade"
}

ImColor GrenadeHelper::getColor(GrenadeType type)
{
	switch (type)
	{
		case GrenadeType::HEGRENADE:
			return Settings::GrenadeHelper::infoHE;
		case GrenadeType::SMOKE:
			return Settings::GrenadeHelper::infoSmoke;
		case GrenadeType::FLASH:
			return Settings::GrenadeHelper::infoFlash;
		case GrenadeType::MOLOTOV:
			return Settings::GrenadeHelper::infoMolotov;
		default:
			return ImColor(255, 255, 255);
	}
}

void GrenadeHelper::DrawGrenadeInfo(GrenadeInfo* info)
{
	Vector pos2d;
	if (debugOverlay->ScreenPosition(Vector(info->pos.x, info->pos.y, info->pos.z), pos2d))
		return;

	Color clr = Color::FromImColor(getColor(info->gType));
	float radius = 20;
	Draw::Circle(Vector2D(pos2d.x, pos2d.y), 15, radius, clr);
	Draw::Text(pos2d.x + radius, pos2d.y, info->name.c_str(), esp_font, clr);
}

void GrenadeHelper::DrawAimHelp(GrenadeInfo* info)
{
	Vector infoVec;
	Math::AngleVectors(info->angle, infoVec);
	infoVec *= 150 / infoVec.Length();
	infoVec += info->pos;

	Vector posVec;
	if (debugOverlay->ScreenPosition(infoVec, posVec))
		return;

	int w, h;
	engine->GetScreenSize(w, h);

	Vector2D pos2d(posVec.x, posVec.y);

	// Draw Point to Throw to
	Draw::FilledCircle(pos2d, 20, 5, Color::FromImColor(Settings::GrenadeHelper::aimDot));

	// Draw Help line
	Draw::Line(Vector2D(w / 2, h / 2), pos2d, Color::FromImColor(Settings::GrenadeHelper::aimLine));
}

void GrenadeHelper::Paint()
{
	if (!Settings::ESP::enabled || !engine->IsInGame() || !Settings::GrenadeHelper::enabled)
		return;

	C_BasePlayer* localPlayer = (C_BasePlayer*) entityList->GetClientEntity(engine->GetLocalPlayer());
	if (!localPlayer)
		return;

	C_BaseCombatWeapon* activeWeapon = (C_BaseCombatWeapon*) entityList->GetClientEntityFromHandle(localPlayer->GetActiveWeapon());
	if (!activeWeapon || activeWeapon->GetCSWpnData()->GetWeaponType() != CSWeaponType::WEAPONTYPE_GRENADE)
		return;

	if (Settings::GrenadeHelper::grenadeInfos.empty())
		return;

	for (auto grenadeInfo = Settings::GrenadeHelper::grenadeInfos.begin(); grenadeInfo != Settings::GrenadeHelper::grenadeInfos.end(); grenadeInfo++)
	{
		if (Settings::GrenadeHelper::onlyMatchingInfos && getGrenadeType(activeWeapon) != grenadeInfo->gType)
			continue;

		float dist = grenadeInfo->pos.DistTo(localPlayer->GetVecOrigin());
		if (dist > 1000)
			continue;

		GrenadeHelper::DrawGrenadeInfo(grenadeInfo.base());

		if (dist < 75)
			GrenadeHelper::DrawAimHelp(grenadeInfo.base());
	}
}

void GrenadeHelper::AimAssist(CUserCmd* cmd)
{
	if (!Settings::GrenadeHelper::aimAssist || !engine->IsInGame() || !Settings::GrenadeHelper::enabled)
		return;

	C_BasePlayer* localPlayer = (C_BasePlayer*) entityList->GetClientEntity(engine->GetLocalPlayer());
	if (!localPlayer || !localPlayer->GetAlive())
		return;

	C_BaseCombatWeapon* activeWeapon = (C_BaseCombatWeapon*) entityList->GetClientEntityFromHandle(localPlayer->GetActiveWeapon());

	if (!activeWeapon || activeWeapon->GetCSWpnData()->GetWeaponType() != CSWeaponType::WEAPONTYPE_GRENADE)
		return;

	bool shootThisTick = cmd->buttons & IN_ATTACK;
	if (!shootThisTick && !shotLastTick)
		return;

	if (Settings::GrenadeHelper::grenadeInfos.empty())
		return;
	//for storing the best result
	GrenadeInfo* grenadeInfo = nullptr;
	float distOnScreen = -1;//Viewangle difference
	float dist = 1000;//3D distance

	int w, h;
	engine->GetScreenSize(w, h);
	Vector ScreenCenter(w / 2, h / 2, 0);

	for (auto act = Settings::GrenadeHelper::grenadeInfos.begin(); act != Settings::GrenadeHelper::grenadeInfos.end(); act++)
	{
		if (Settings::GrenadeHelper::onlyMatchingInfos && getGrenadeType(activeWeapon) != act->gType)
			continue;

		float dist3D = Math::GetDistance(localPlayer->GetEyePosition(), act->pos);
		if (dist3D > 75.f)
			continue;

		float actDistOnScreen;
		Vector infoVec;
		Math::AngleVectors(act->angle, infoVec);
		infoVec *= 150 / infoVec.Length();
		infoVec += act->pos;
		Vector posVec;
		if (debugOverlay->ScreenPosition(infoVec, posVec))
			actDistOnScreen = 180;
		else
			actDistOnScreen = (posVec - ScreenCenter).Length();

		if (distOnScreen != -1 &&  distOnScreen < actDistOnScreen)
			continue;
		grenadeInfo = act.base();
		distOnScreen = actDistOnScreen;
		dist = dist3D;
	}
	if (!grenadeInfo)
		return;

	if (!shootThisTick && shotLastTick && dist < 5)
	{
		//throw the grenade
		if (grenadeInfo->tType == ThrowType::JUMP)
			cmd->buttons |= IN_JUMP;
		if (grenadeInfo->tType == ThrowType::WALK)
			cmd->buttons |= IN_WALK;
		engine->SetViewAngles(grenadeInfo->angle);
	}
	else
	{
		if (dist > 0.5f)
		{
			QAngle movement = Math::CalcAngle(localPlayer->GetEyePosition(), grenadeInfo->pos);
			if (cmd->forwardmove < dist) cmd->forwardmove = dist * 2;
			cmd->sidemove = 0;
			cmd->buttons |= IN_WALK;
			Math::CorrectMovement(movement, cmd, cmd->forwardmove, cmd->sidemove);
		}
		if (cmd->viewangles !=  grenadeInfo->angle)
		{
			//Aim towards the aimspot
			float maxStep = 5;
			QAngle old = cmd->viewangles;

			float diffX = grenadeInfo->angle.x - old.x;
			if (diffX > maxStep) diffX = maxStep;
			if (diffX < -maxStep) diffX = -maxStep;
			float diffY = grenadeInfo->angle.y - old.y;
			while (diffY > 180) diffY -= 360;
			while (diffY < -180) diffY += 360;
			if (diffY > maxStep) diffY = maxStep;
			if (diffY < -maxStep) diffY = -maxStep;
			cmd->viewangles += QAngle(diffX, diffY, 0);
			Math::ClampAngles(cmd->viewangles);
			engine->SetViewAngles(cmd->viewangles);
			Math::CorrectMovement(old, cmd, cmd->forwardmove, cmd->sidemove);
		}
	}
}

void GrenadeHelper::CheckForUpdate()
{
	if (!engine->IsInGame())
		return;

	pstring s = pstring(GetLocalClient(-1)->m_szLevelNameShort);
	unsigned long p = s.find_last_of("/");
	if (p != std::string::npos)
		s.erase(0, p + 1);

	if (!Settings::GrenadeHelper::actMapName.compare(s))
		return;

	Settings::GrenadeHelper::actMapName = s;
	pstring path = GetGhConfigDirectory().append(s).append("/config.json");

	if (DoesFileExist(path.c_str()))
		Settings::LoadGrenadeInfo(path);
	else
		Settings::GrenadeHelper::grenadeInfos = {};
}

void GrenadeHelper::CreateMove(CUserCmd* cmd)
{
	GrenadeHelper::CheckForUpdate();
	GrenadeHelper::AimAssist(cmd);
	shotLastTick = cmd->buttons & IN_ATTACK;
}