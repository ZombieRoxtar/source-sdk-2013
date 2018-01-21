//========================================================================//
//
// Purpose: The system for an extenal map patch file. Stolen from commentary.
//
//========================================================================//
#include "cbase.h"
#include "filesystem.h"
#include "saverestore_utlvector.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar game_allow_patches("game_allow_patches", "1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX, "Allow external patching.");

#define PATCH_FILE "maps/%s_patch.txt"
#define PATCH_STRING_LENGTH_MAX 1024

//========================================================================
class CPatchSystem : public CAutoGameSystemPerFrame
{
private:
	bool g_bPatchMode;
	CUtlVector<EHANDLE>	m_hSpawnedEntities;

	DECLARE_DATADESC();

	//-----------------------------------------------------------------------------
	virtual void LevelInitPreEntity()
	{
		g_bPatchMode = game_allow_patches.GetBool();
	}

	//-----------------------------------------------------------------------------
	void ParseEntKVBlock(CBaseEntity *pNode, KeyValues *pkvNode)
	{
		KeyValues *pkvNodeData = pkvNode->GetFirstSubKey();
		while (pkvNodeData)
		{
			// Handle the connections block
			if (!Q_strcmp(pkvNodeData->GetName(), "connections"))
			{
				ParseEntKVBlock(pNode, pkvNodeData);
			}
			else
			{
				const char *pszValue = pkvNodeData->GetString();
				Assert(Q_strlen(pszValue) < PATCH_STRING_LENGTH_MAX);
				if (Q_strnchr(pszValue, '^', PATCH_STRING_LENGTH_MAX))
				{
					// We want to support quotes in our strings so that we can specify multiple parameters in
					// an output inside our files. We convert ^s to "s here.
					char szTmp[PATCH_STRING_LENGTH_MAX];
					Q_strncpy(szTmp, pszValue, PATCH_STRING_LENGTH_MAX);
					int len = Q_strlen(szTmp);
					for (int i = 0; i < len; i++)
					{
						if (szTmp[i] == '^')
						{
							szTmp[i] = '"';
						}
					}
					pNode->KeyValue(pkvNodeData->GetName(), szTmp);
				}
				else
				{
					pNode->KeyValue(pkvNodeData->GetName(), pszValue);
				}
			}
			pkvNodeData = pkvNodeData->GetNextKey();
		}
	}
	//-----------------------------------------------------------------------------
	virtual void LevelInitPostEntity(void)
	{
		if (!g_bPatchMode)
			return;

		// Don't spawn entities when loading a savegame
		if (gpGlobals->eLoadType == MapLoad_LoadGame)
			return;

		InitPatch();
	}

	//-----------------------------------------------------------------------------
	void InitPatch(void)
	{
		bool oldLock = engine->LockNetworkStringTables(false);

		char szFullName[512];
		Q_snprintf(szFullName, sizeof(szFullName), PATCH_FILE, STRING(gpGlobals->mapname));
		KeyValues *pkvFile = new KeyValues("PatchFile");
		if (pkvFile->LoadFromFile(filesystem, szFullName, "MOD"))
		{
			// Load each block, and spawn the entities
			KeyValues *pkvNode = pkvFile->GetFirstSubKey();
			while (pkvNode)
			{
				// Get node name
				const char *pNodeName = pkvNode->GetName();

				KeyValues *pClassname = pkvNode->FindKey("classname");
				if (pClassname)
				{
					// Use the classname instead
					pNodeName = pClassname->GetString();
				}
				// Spawn the entity
				CBaseEntity *pNode = CreateEntityByName(pNodeName);
				if (pNode)
				{
					ParseEntKVBlock(pNode, pkvNode);
					DispatchSpawn(pNode);

					EHANDLE hHandle;
					hHandle = pNode;
					m_hSpawnedEntities.AddToTail(hHandle);
				}
				else
				{
					Warning("Patching: Failed to spawn entity, type: '%s'\n", pNodeName);
				}
				pkvFile->deleteThis();
				// Move to next entity
				pkvNode = pkvNode->GetNextKey();
			}
			// Then activate all the entities
			for (int i = 0; i < m_hSpawnedEntities.Count(); i++)
			{
				m_hSpawnedEntities[i]->Activate();
			}
		}
		engine->LockNetworkStringTables(oldLock);
	}
};
//-----------------------------------------------------------------------------
CPatchSystem g_PatchSystem;

BEGIN_DATADESC_NO_BASE(CPatchSystem)
	DEFINE_UTLVECTOR(m_hSpawnedEntities, FIELD_EHANDLE),
END_DATADESC()
