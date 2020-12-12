//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a combine ball and a combine ball launcher which have certain properties
//			overwritten to make use of them in portal game play.
//
//========================================================================//

#ifndef PROP_ENERGY_BALL_H
#define PROP_ENERGY_BALL_H

#ifdef _WIN32
#pragma once
#endif

#include "prop_combine_ball.h" // For the base class.

class CPropEnergyBall : public CPropCombineBall
{
public:
	DECLARE_CLASS(CPropEnergyBall, CPropCombineBall);
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void Precache();
	virtual void CreateSounds(void);
	virtual void StopLoopingSounds(void);
	virtual void Spawn();
	virtual void Activate(void);

	// Overload for unlimited bounces and predictable movement
	virtual void VPhysicsCollision(int index, gamevcollisionevent_t* pEvent);
	// Overload for less sound, no shake.
	virtual void ExplodeThink(void);
	// Update in a time till death update
	virtual void Think(void);
	virtual void EndTouch(CBaseEntity* pOther);
	virtual void StartTouch(CBaseEntity* pOther);
	virtual void NotifySystemEvent(CBaseEntity* pNotify, notify_system_event_t eventType, const notify_system_event_params_t& params);

	CHandle<CProp_Portal>	m_hTouchedPortal;	// Pointer to the portal we are touched most recently
	bool					m_bTouchingPortal1;	// Are we touching portal 1
	bool					m_bTouchingPortal2;	// Are we touching portal 2

	// Remember the last known direction of travel, incase our velocity is cleared.
	Vector					m_vLastKnownDirection;

	// After portal teleports, we force the life to be at least this number.
	float					m_fMinLifeAfterPortal;

	CNetworkVar(bool, m_bIsInfiniteLife);
	CNetworkVar(float, m_fTimeTillDeath);

	CSoundPatch* m_pAmbientSound;

};

#endif //PROP_ENERGY_BALL_H