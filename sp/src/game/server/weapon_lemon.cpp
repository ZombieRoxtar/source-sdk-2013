//========= Source SDK License, All rights reserved. ============//
//
// Purpose: Debug weapon that fires rockets and energy balls.
// Science: Isn't about "Why?" It's about "Why not?"
//
//===============================================================//
#include "cbase.h"
#include "soundent.h"
#include "weapon_rpg.h"
#include "prop_energy_ball.h"
#include "triggers.h"
#include "rumble_shared.h"
#include "gamestats.h"

#ifdef PORTAL
#include "portal_util_shared.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//============================================
// Lemon-Powered RPG!
//============================================
class CWeaponLemon : public CBaseHLCombatWeapon
{
	DECLARE_CLASS(CWeaponLemon, CBaseHLCombatWeapon);

public:
	CWeaponLemon();

	DECLARE_SERVERCLASS();

	void Precache(void);
	void PrimaryAttack(void);
	void SecondaryAttack(void);
	void DelayedAttack(void);
	void ItemPostFrame(void);

	bool Holster(CBaseCombatWeapon* pSwitchingTo = NULL);
	CPropEnergyBall* FireEnergyBall(void);

	DECLARE_ACTTABLE();
	DECLARE_DATADESC();

protected:
	bool			  m_bShotDelayed;  // To prevent firing while firing
	float			  m_flDelayedFire; // The time to create the energy ball
	CHandle<CMissile> m_hMissile;
};

BEGIN_DATADESC(CWeaponLemon)
	DEFINE_FIELD(m_flDelayedFire, FIELD_TIME),
	DEFINE_FIELD(m_bShotDelayed, FIELD_BOOLEAN),
	DEFINE_FIELD(m_hMissile, FIELD_EHANDLE),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponLemon, DT_WeaponLemon)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_lemon, CWeaponLemon);
PRECACHE_WEAPON_REGISTER(weapon_lemon);

acttable_t	CWeaponLemon::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1,	ACT_RANGE_ATTACK_AR2,		true },
	{ ACT_IDLE_RELAXED,		ACT_IDLE_SMG1_RELAXED,		false},
	{ ACT_IDLE_STIMULATED,	ACT_IDLE_SMG1_STIMULATED,	false},
	{ ACT_IDLE_AGITATED,	ACT_IDLE_ANGRY_SMG1,		false},
	{ ACT_IDLE,				ACT_IDLE_SMG1,				true },
	{ ACT_IDLE_ANGRY,		ACT_IDLE_ANGRY_SMG1,		true },
	{ ACT_WALK,				ACT_WALK_RIFLE,				true },
	{ ACT_WALK_CROUCH,		ACT_WALK_CROUCH_RIFLE,		true },
	{ ACT_RUN,				ACT_RUN_RIFLE,				true },
	{ ACT_RUN_CROUCH,		ACT_RUN_CROUCH_RIFLE,		true },
	{ ACT_COVER_LOW,		ACT_COVER_SMG1_LOW,			false},
};
IMPLEMENT_ACTTABLE(CWeaponLemon);

//-----------------------------------------------------------------------------
CWeaponLemon::CWeaponLemon()
{
	m_fMinRange1 = m_fMinRange2 = 480;
	m_fMaxRange1 = m_fMaxRange2 = 6000;
}

//-----------------------------------------------------------------------------
void CWeaponLemon::Precache(void)
{
	BaseClass::Precache();

	PrecacheScriptSound("Missile.Ignite");
	PrecacheScriptSound("Missile.Accelerate");
	UTIL_PrecacheOther("rpg_missile");
}

//-----------------------------------------------------------------------------
// Purpose: Fire RPG
//-----------------------------------------------------------------------------
void CWeaponLemon::PrimaryAttack(void)
{
	// Can't have an active missile out
	if (m_hMissile != NULL)
		return;

	// Can't be reloading
	if (GetActivity() == ACT_VM_RELOAD)
		return;

	Vector vecOrigin;
	Vector vecForward;

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	Vector	vForward, vRight, vUp;

	pOwner->EyeVectors(&vForward, &vRight, &vUp);

	Vector	muzzlePoint = pOwner->Weapon_ShootPosition() + vForward * 12.0f + vRight * 6.0f + vUp * -3.0f;

	QAngle vecAngles;
	VectorAngles(vForward, vecAngles);
	m_hMissile = CMissile::Create(muzzlePoint, vecAngles, GetOwner()->edict());

	// If the shot is clear to the player, give the missile a grace period
	trace_t	tr;
	Vector vecEye = pOwner->EyePosition();
	UTIL_TraceLine(vecEye, vecEye + vForward * 128, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction == 1.0)
	{
		m_hMissile->SetGracePeriod(0.3);
	}

	// Register a muzzleflash for the AI
	pOwner->SetMuzzleFlashTime(gpGlobals->curtime + 0.5);

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	WeaponSound(SINGLE);

	pOwner->RumbleEffect(RUMBLE_SHOTGUN_SINGLE, 0, RUMBLE_FLAG_RESTART);

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pOwner, true, GetClassname());

	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 1000, 0.2, GetOwner(), SOUNDENT_CHANNEL_WEAPON);

	// Check to see if we should trigger any RPG firing triggers
	int iCount = g_hWeaponFireTriggers.Count();
	for (int i = 0; i < iCount; i++)
	{
		if (g_hWeaponFireTriggers[i]->IsTouching(pOwner))
		{
			if (FClassnameIs(g_hWeaponFireTriggers[i], "trigger_rpgfire"))
			{
				g_hWeaponFireTriggers[i]->ActivateMultiTrigger(pOwner);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Logic at end frame
//-----------------------------------------------------------------------------
void CWeaponLemon::ItemPostFrame(void)
{
	// See if we need to fire off our secondary round
	if (m_bShotDelayed && gpGlobals->curtime > m_flDelayedFire)
	{
		DelayedAttack();
	}
	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Holster if not firing
//-----------------------------------------------------------------------------
bool CWeaponLemon::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	if (m_bShotDelayed)
		return false;

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Starts the energy ball attack
//-----------------------------------------------------------------------------
void CWeaponLemon::SecondaryAttack(void)
{
	if (m_bShotDelayed)
		return;

	m_bShotDelayed = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flDelayedFire = gpGlobals->curtime + 0.5f;

	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer)
	{
		pPlayer->RumbleEffect(RUMBLE_AR2_ALT_FIRE, 0, RUMBLE_FLAG_RESTART);
	}

	SendWeaponAnim(ACT_VM_FIDGET);
	WeaponSound(SPECIAL1);

	m_iSecondaryAttacks++;
	gamestats->Event_WeaponFired(pPlayer, false, GetClassname());
}

//-----------------------------------------------------------------------------
// Purpose: Fires the energy ball.
//-----------------------------------------------------------------------------
void CWeaponLemon::DelayedAttack(void)
{
	m_bShotDelayed = false;

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	SendWeaponAnim(ACT_VM_SECONDARYATTACK);
	m_flNextSecondaryAttack = pOwner->m_flNextAttack = gpGlobals->curtime + SequenceDuration();

	// Register a muzzleflash for the AI
	pOwner->DoMuzzleFlash();
	pOwner->SetMuzzleFlashTime(gpGlobals->curtime + 0.5);

	WeaponSound(WPN_DOUBLE);

	pOwner->RumbleEffect(RUMBLE_SHOTGUN_DOUBLE, 0, RUMBLE_FLAG_RESTART);

	FireEnergyBall();

	// View effects
	color32 white = { 255, 255, 255, 64 };
	UTIL_ScreenFade(pOwner, white, 0.1, 0, FFADE_IN);

	//Disorient the player
	QAngle angles = pOwner->GetLocalAngles();

	angles.x += random->RandomInt(-4, 4);
	angles.y += random->RandomInt(-4, 4);
	angles.z = 0;

	pOwner->SnapEyeAngles(angles);

	pOwner->ViewPunch(QAngle(random->RandomInt(-8, -12), random->RandomInt(1, 2), 0));

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
}

CPropEnergyBall* CWeaponLemon::FireEnergyBall(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer == NULL)
		return NULL;

	Vector ptEyes, vForward;
	ptEyes = pPlayer->EyePosition();
	pPlayer->EyeVectors(&vForward);

	CPropEnergyBall* pBall = static_cast<CPropEnergyBall*>(CreateEntityByName("prop_energy_ball"));
	if (!pBall)
		return NULL;

	pBall->SetRadius(12.0f);
	pBall->SetAbsOrigin(ptEyes + (vForward * 50.0f));
	pBall->SetSpawner(NULL);
	pBall->SetSpeed(400.0f);
	pBall->SetAbsVelocity(vForward * 400.0f);
	DispatchSpawn(pBall);
	pBall->Activate();
	pBall->SetState(CPropCombineBall::STATE_LAUNCHED);
	pBall->SetCollisionGroup(COLLISION_GROUP_PROJECTILE);
	pBall->m_fMinLifeAfterPortal = 5.0f;

	// Additional setup of the physics object for energy ball uses
	IPhysicsObject* pBallObj = pBall->VPhysicsGetObject();

	if (pBallObj)
	{
		// Make sure we dont use air drag
		pBallObj->EnableDrag(false);

		// Remove damping
		float speed, rot;
		speed = rot = 0.0f;
		pBallObj->SetDamping(&speed, &rot);

		// HUGE rotational inertia, don't allow the ball to have any spin
		Vector vInertia(1e30, 1e30, 1e30);
		pBallObj->SetInertia(vInertia);

		// Low mass to let it bounce off of obstructions for certain puzzles.
		pBallObj->SetMass(1.0f);
	}
	pBall->StartLifetime(10.0f);
	pBall->m_bIsInfiniteLife = false;

	// Think function, used to update time till death and avoid sleeping
	pBall->SetNextThink(gpGlobals->curtime + 0.1f);

	return pBall;
}